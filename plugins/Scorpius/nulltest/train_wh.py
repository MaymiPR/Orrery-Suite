"""Learns a module of the reference as a Wiener-Hammerstein model with trainable parts (PyTorch, CPU):
    y = Post(N(Pre(x)))
Pre a causal FIR (starts as a unit impulse), N a static shaper (an asymmetric soft clip with learned drive, ceiling,
bias and knee, plus a small MLP correction that starts at zero), Post a long causal FIR (starts as the measured
small-signal impulse response). Trained on capture_nn.py's material (mean squared error relative to each piece's
level), validated by nulls on the held-out pieces. Writes measurements/scorpius/nn/<case>_model.pt and a JSON copy.
    python train_wh.py <case> [steps] [ir name]"""
import json, os, sys, time
import numpy as np
import torch
import orion as o

FS = 48000
NN = os.path.join(o.ROOT, 'measurements', 'scorpius', 'nn')
IR = os.path.join(o.ROOT, 'measurements', 'scorpius', 'ir')
KPRE, KPOST, SEG, BATCH = 256, 8192, 8192, 12
torch.set_num_threads(max(1, os.cpu_count() // 2))


def fftconv(x, h):
    """causal convolution of (batch, n) with (k,), output (batch, n)"""
    n = x.shape[-1] + h.shape[-1]
    m = 1 << int(np.ceil(np.log2(n)))
    return torch.fft.irfft(torch.fft.rfft(x, m) * torch.fft.rfft(h, m), m)[..., :x.shape[-1]]


class Model(torch.nn.Module):
    def __init__(self, post_init):
        super().__init__()
        pre = torch.zeros(KPRE); pre[0] = 1.0
        self.pre = torch.nn.Parameter(pre)
        self.post = torch.nn.Parameter(torch.tensor(post_init[:KPOST], dtype=torch.float32))
        self.g = torch.nn.Parameter(torch.tensor(1.5)); self.c = torch.nn.Parameter(torch.tensor(0.6))
        self.b = torch.nn.Parameter(torch.tensor(-0.05)); self.lp = torch.nn.Parameter(torch.tensor(np.log(4.0), dtype=torch.float32))
        self.mlp = torch.nn.Sequential(torch.nn.Linear(1, 16), torch.nn.Tanh(), torch.nn.Linear(16, 16), torch.nn.Tanh(), torch.nn.Linear(16, 1))
        with torch.no_grad():
            self.mlp[-1].weight.zero_(); self.mlp[-1].bias.zero_()

    def shaper(self, u):
        p = torch.exp(self.lp); c = torch.abs(self.c) + 1e-3
        s = lambda v: v / (1 + torch.abs(v) ** p) ** (1 / p)
        ds = (1 + torch.abs(self.b / c) ** p) ** (-1 - 1 / p)
        v = self.g * u
        clip = c * (s(v / c + self.b / c) - s(self.b / c)) / ds / self.g
        return clip + 0.1 * self.mlp(u.unsqueeze(-1)).squeeze(-1)

    def forward(self, x):
        return fftconv(self.shaper(fftconv(x, self.pre)), self.post)


def load(case):
    d = np.load(os.path.join(NN, case + '.npz'), allow_pickle=True)
    return list(d['x']), list(d['y']), list(d['valid'])


def train(case, steps, irname):
    xs, ys, valid = load(case)
    h = np.load(os.path.join(IR, irname + '.npy'))[0].astype(np.float32)
    model = Model(h)
    train_idx = [i for i, v in enumerate(valid) if not v]
    opt = torch.optim.Adam([{'params': [model.pre, model.post], 'lr': 2e-4},
                            {'params': [model.g, model.c, model.b, model.lp] + list(model.mlp.parameters()), 'lr': 3e-3}])
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, steps)
    rng = np.random.default_rng(1)
    warm = KPRE + KPOST
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
        opt.zero_grad(); loss.backward(); opt.step(); sched.step()
        if step % 200 == 0 or step == steps - 1:
            print(f'{case} step {step}: loss {10*np.log10(loss.item()):.1f} dB ({time.time()-t0:.0f} s)', flush=True)
    return model, xs, ys, valid


def evaluate(model, xs, ys, valid):
    res = []
    with torch.no_grad():
        for x, y, v in zip(xs, ys, valid):
            if not v: continue
            out = model(torch.tensor(x[None]))[0].numpy()
            res.append(o.null_db(y[None], out[None]))
    return res


if __name__ == '__main__':
    case = sys.argv[1]; steps = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    irname = sys.argv[3] if len(sys.argv) > 3 else {'lift': 'lift_1.0'}.get(case, case.replace('drive_', 'drive_').replace('_hp', '') + ('_hp1' if case.endswith('_hp') else '_hp0'))
    if case.startswith('drive_') and not case.endswith('_hp'): irname = f'drive_{case[6]}_hp0'
    if case.startswith('drive_') and case.endswith('_hp'): irname = f'drive_{case[6]}_hp1'
    model, xs, ys, valid = train(case, steps, irname)
    n = evaluate(model, xs, ys, valid)
    print(case, 'validation nulls', ' '.join('%.1f' % v for v in n), 'mean %.1f' % np.mean(n))
    torch.save(model.state_dict(), os.path.join(NN, case + '_model.pt'))
    json.dump(dict(pre=model.pre.detach().tolist(), post=model.post.detach().tolist(), g=model.g.item(), c=model.c.item(),
                   b=model.b.item(), p=float(torch.exp(model.lp)), mlp=[p.detach().tolist() for p in model.mlp.parameters()],
                   valid=n), open(os.path.join(NN, case + '_model.json'), 'w'))
