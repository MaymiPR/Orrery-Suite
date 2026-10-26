"""Learns a module of the reference with a small GRU between two learned linear filters (PyTorch, CPU):
    u = Pre(x);  v = u + W GRU(u);  y = Post(v)
The GRU runs over a short warm-up and the loss window only; before that the long Post filter is fed u itself (the
nonlinear part's history matters little to it). Material as train_wn.py.
    python train_gru.py <case> [steps] [hidden]"""
import os, sys, time
import numpy as np
import torch
import orion as o
from train_wh import fftconv, NN, IR, load
from train_wn import irname_of, MAXPEAK

KPRE, KPOST, SEG, GWARM, BATCH = 64, 8192, 2048, 512, 16
torch.set_num_threads(max(1, os.cpu_count() // 2))


class Model(torch.nn.Module):
    def __init__(self, post_init, hidden):
        super().__init__()
        pre = torch.zeros(KPRE); pre[0] = 1.0
        self.pre = torch.nn.Parameter(pre)
        self.post = torch.nn.Parameter(torch.tensor(post_init[:KPOST], dtype=torch.float32))
        self.gru = torch.nn.GRU(1, hidden, batch_first=True)
        self.out = torch.nn.Linear(hidden, 1)
        with torch.no_grad(): self.out.weight.zero_(); self.out.bias.zero_()

    def core(self, u):                      # (batch, n) -> (batch, n)
        h, _ = self.gru(u.unsqueeze(-1))
        return u + self.out(h).squeeze(-1)

    def forward(self, x, gru_from=0):
        u = fftconv(x, self.pre)
        if gru_from:
            v = torch.cat([u[:, :gru_from], self.core(u[:, gru_from:])], 1)
        else:
            v = self.core(u)
        return fftconv(v, self.post)


def train(case, steps, hidden):
    xs, ys, valid = load(case)
    keep = [i for i, x in enumerate(xs) if 20 * np.log10(np.abs(x).max() + 1e-9) <= MAXPEAK]
    h = np.load(os.path.join(IR, irname_of(case) + '.npy'))[0].astype(np.float32)
    model = Model(h, hidden)
    train_idx = [i for i in keep if not valid[i]]
    opt = torch.optim.Adam([{'params': [model.pre, model.post], 'lr': 1e-4},
                            {'params': list(model.gru.parameters()) + list(model.out.parameters()), 'lr': 3e-3}])
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=[1e-4, 5e-3], total_steps=steps)
    rng = np.random.default_rng(2)
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
        out = model(xb, gru_from=warm - GWARM)[:, warm:]
        loss = (((out - yb) ** 2).mean(1) * wb).mean()
        opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0); opt.step(); sched.step()
        if step % 500 == 0 or step == steps - 1:
            print(f'{case} step {step}: loss {10*np.log10(loss.item()):.1f} dB ({time.time()-t0:.0f} s)', flush=True)
    return model, xs, ys, valid, keep


def evaluate(model, xs, ys, valid, keep):
    res = []
    with torch.no_grad():
        for i, (x, y, v) in enumerate(zip(xs, ys, valid)):
            if i not in keep or not v: continue
            out = model(torch.tensor(x[None]))[0].numpy()
            res.append((i, o.null_db(y[None], out[None])))
    return res


if __name__ == '__main__':
    case = sys.argv[1]; steps = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
    hidden = int(sys.argv[3]) if len(sys.argv) > 3 else 16
    model, xs, ys, valid, keep = train(case, steps, hidden)
    n = evaluate(model, xs, ys, valid, keep)
    print(case, f'hidden {hidden}', 'validation nulls', ' '.join('%d:%.1f' % v for v in n), 'mean %.1f' % np.mean([v for _, v in n]))
    torch.save(dict(state=model.state_dict(), hidden=hidden), os.path.join(NN, case + f'_gru{hidden}.pt'))
