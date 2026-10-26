"""Learns a module of the reference with a small WaveNet (dilated causal convolutions, gated) between two learned linear
filters (PyTorch, CPU):
    u = Pre(x);  v = u + WaveNet(u);  y = Post(v)
Pre a short causal FIR (starts as a unit impulse), Post a long causal FIR (starts as the measured small-signal impulse
response), the WaveNet learning what the filters cannot (its output starts at zero). Trained on capture_nn.py's
material up to `maxpeak` dBFS (the reference's own behaviour is erratic far above full scale), validated by nulls on
the held-out pieces. Writes measurements/scorpius/nn/<case>_wn.pt.
    python train_wn.py <case> [steps] [channels] [layers]"""
import os, sys, time
import numpy as np
import torch
import orion as o
from train_wh import fftconv, NN, IR, load

FS = 48000
KPRE, KPOST, SEG, BATCH = 64, 8192, 4096, 16
MAXPEAK = 4.0          # dBFS: training and validation pieces up to this peak
torch.set_num_threads(int(os.environ.get("THREADS", max(1, os.cpu_count() // 2))))


class WaveNet(torch.nn.Module):
    def __init__(self, ch=8, layers=10):
        super().__init__()
        self.dil = [2 ** (i % 12) for i in range(layers)]
        self.inp = torch.nn.Conv1d(1, ch, 1)
        self.conv = torch.nn.ModuleList([torch.nn.Conv1d(ch, 2 * ch, 2, dilation=d) for d in self.dil])
        self.res = torch.nn.ModuleList([torch.nn.Conv1d(ch, ch, 1) for _ in self.dil])
        self.skip = torch.nn.ModuleList([torch.nn.Conv1d(ch, ch, 1) for _ in self.dil])
        self.out = torch.nn.Conv1d(ch, 1, 1)
        with torch.no_grad(): self.out.weight.zero_(); self.out.bias.zero_()
        self.rf = sum(self.dil) + 1

    def forward(self, u):                     # (batch, n) -> (batch, n), causal
        h = self.inp(u.unsqueeze(1)); s = 0
        for d, conv, res, skip in zip(self.dil, self.conv, self.res, self.skip):
            z = conv(torch.nn.functional.pad(h, (d, 0)))
            a, b = z.chunk(2, 1)
            g = torch.tanh(a) * torch.sigmoid(b)
            h = h + res(g); s = s + skip(g)
        return self.out(torch.relu(s)).squeeze(1)


class Model(torch.nn.Module):
    def __init__(self, post_init, ch, layers):
        super().__init__()
        pre = torch.zeros(KPRE); pre[0] = 1.0
        self.pre = torch.nn.Parameter(pre)
        self.post = torch.nn.Parameter(torch.tensor(post_init[:KPOST], dtype=torch.float32))
        self.wn = WaveNet(ch, layers)

    def forward(self, x):
        u = fftconv(x, self.pre)
        return fftconv(u + self.wn(u), self.post)


def train(case, steps, irname, ch, layers):
    xs, ys, valid = load(case)
    keep = [i for i, x in enumerate(xs) if 20 * np.log10(np.abs(x).max() + 1e-9) <= MAXPEAK]
    h = np.load(os.path.join(IR, irname + '.npy'))[0].astype(np.float32)
    model = Model(h, ch, layers)
    train_idx = [i for i in keep if not valid[i]]
    opt = torch.optim.Adam([{'params': [model.pre, model.post], 'lr': 1e-4}, {'params': model.wn.parameters(), 'lr': 2e-3}])
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=[1e-4, 3e-3], total_steps=steps)
    rng = np.random.default_rng(1)
    warm = KPRE + KPOST + model.wn.rf
    t0 = time.time()
    for step in range(steps):
        xb, yb, wb = [], [], []
        for _ in range(BATCH):
            i = train_idx[rng.integers(len(train_idx))]
            x, y = xs[i], ys[i]
            start = rng.integers(warm, len(x) - SEG)
            xb.append(x[start - warm:start + SEG]); yb.append(y[start:start + SEG])
            wb.append(1.0 / max(1e-6, float(np.mean(y[start:start + SEG] ** 2))))
        xb = torch.tensor(np.array(xb)); yb = torch.tensor(np.array(yb)); wb = torch.tensor(wb, dtype=torch.float32)
        out = model(xb)[:, warm:]
        loss = (((out - yb) ** 2).mean(1) * wb).mean()
        opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0); opt.step(); sched.step()
        if step % 500 == 0 or step == steps - 1:
            print(f'{case} step {step}: loss {10*np.log10(loss.item()):.1f} dB ({time.time()-t0:.0f} s)', flush=True)
    return model, xs, ys, valid, keep


def evaluate(model, xs, ys, valid, keep, all_=False):
    res = []
    with torch.no_grad():
        for i, (x, y, v) in enumerate(zip(xs, ys, valid)):
            if i not in keep or (not v and not all_): continue
            out = model(torch.tensor(x[None]))[0].numpy()
            res.append((i, o.null_db(y[None], out[None])))
    return res


def irname_of(case):
    if case == 'lift': return 'lift_1.0'
    return f'drive_{case[6]}_hp{1 if case.endswith("_hp") else 0}'


if __name__ == '__main__':
    case = sys.argv[1]; steps = int(sys.argv[2]) if len(sys.argv) > 2 else 6000
    ch = int(sys.argv[3]) if len(sys.argv) > 3 else 8; layers = int(sys.argv[4]) if len(sys.argv) > 4 else 10
    model, xs, ys, valid, keep = train(case, steps, irname_of(case), ch, layers)
    n = evaluate(model, xs, ys, valid, keep)
    print(case, f'ch {ch} layers {layers}', 'validation nulls', ' '.join('%d:%.1f' % v for v in n), 'mean %.1f' % np.mean([v for _, v in n]))
    torch.save(dict(state=model.state_dict(), ch=ch, layers=layers), os.path.join(NN, case + '_wn.pt'))
