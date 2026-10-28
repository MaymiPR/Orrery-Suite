"""Learns a module of the reference with a small GRU between two learned linear filters (PyTorch; on the GPU when there is one):
    u = Pre(x);  v = u + W GRU(u);  y = Post(v)
The GRU runs over a short warm-up and the loss window only; before that the long Post filter is fed u itself (the
nonlinear part's history matters little to it). Material as train_wn.py. GWARM (environment): the GRU's warm-up in
samples, longer for modules with slow envelopes (grit: 9600). Grit's linear filters start as the identity.
    python train_gru.py <case> [steps] [hidden]"""
import os, sys, time
import numpy as np
import torch
import orion as o
from train_wh import fftconv, NN, IR, load
from train_wn import irname_of, MAXPEAK

KPRE, KPOST, SEG, GWARM, BATCH = 64, 8192, 2048, int(os.environ.get("GWARM", 512)), 16
torch.set_num_threads(int(os.environ.get("THREADS", max(1, os.cpu_count() // 2))))
DEV = torch.device("cuda" if torch.cuda.is_available() else "cpu")
torch.backends.cudnn.allow_tf32 = torch.backends.cuda.matmul.allow_tf32 = False    # (full float precision, as on the CPU)
torch.backends.cudnn.enabled = os.environ.get("CUDNN", "1") == "1"   # (CUDNN=0: PyTorch's own kernels, e.g. for sequences cuDNN rejects)
CELL = os.environ.get("CELL", "gru")       # gru or lstm (the NAM models in the reference are LSTMs)
RNN_LAYERS = int(os.environ.get("LAYERS", 1))   # stacked layers (the reference names an 'LSTM_3_LAYER' drive model)
# STATEWARM: samples the network runs without gradients before its graded warm-up, so each window starts from the state
# a long stretch of the material leaves (a capture's network runs continuously; from a zero state 512 samples before
# the window it learns another regime: REA notes 2026-10-28, an inference)
STATEWARM = int(os.environ.get("STATEWARM", 0))


class Model(torch.nn.Module):
    def __init__(self, post_init, hidden):
        super().__init__()
        pre = torch.zeros(KPRE); pre[0] = 1.0
        self.pre = torch.nn.Parameter(pre)
        self.post = torch.nn.Parameter(torch.tensor(post_init[:KPOST], dtype=torch.float32))
        self.gru = (torch.nn.LSTM if CELL == 'lstm' else torch.nn.GRU)(1, hidden, num_layers=RNN_LAYERS, batch_first=True)
        self.out = torch.nn.Linear(hidden, 1)
        with torch.no_grad(): self.out.weight.zero_(); self.out.bias.zero_()

    def run(self, x, state=None):           # (batch, n, 1) in chunks, the state carried (cuDNN rejects long sequences)
        hs = []
        for k in range(0, x.shape[1], 8192):
            hk, state = self.gru(x[:, k:k + 8192].contiguous(), state); hs.append(hk)
        return torch.cat(hs, 1), state

    def core(self, u, nograd=0):            # (batch, n) -> (batch, n); the first nograd samples without gradients
        x = u.contiguous().unsqueeze(-1)
        if nograd:
            with torch.no_grad():
                h0, state = self.run(x[:, :nograd])
                v0 = u[:, :nograd] + self.out(h0).squeeze(-1)
            h1, _ = self.run(x[:, nograd:], tuple(t.detach() for t in state) if isinstance(state, tuple) else state.detach())
            return torch.cat([v0, u[:, nograd:] + self.out(h1).squeeze(-1)], 1)
        if x.shape[1] <= 16384:
            h, _ = self.gru(x)
        else:                               # long pieces in chunks, the state carried (cuDNN rejects long sequences)
            hs, state = [], None
            for k in range(0, x.shape[1], 8192):
                hk, state = self.gru(x[:, k:k + 8192].contiguous(), state); hs.append(hk)
            h = torch.cat(hs, 1)
        return u + self.out(h).squeeze(-1)

    def forward(self, x, gru_from=0, nograd=0):
        u = fftconv(x, self.pre)
        if gru_from:
            v = torch.cat([u[:, :gru_from], self.core(u[:, gru_from:], nograd)], 1)
        else:
            v = self.core(u)
        return fftconv(v, self.post)


def train(case, steps, hidden):
    xs, ys, valid = load(case)
    keep = [i for i, x in enumerate(xs) if 20 * np.log10(np.abs(x).max() + 1e-9) <= MAXPEAK]
    if irname_of(case) is None:
        h = np.zeros(KPOST, np.float32); h[0] = 1.0
    else:
        h = np.load(os.path.join(IR, irname_of(case) + '.npy'))[0].astype(np.float32)
    model = Model(h, hidden).to(DEV)
    train_idx = [i for i in keep if not valid[i]]
    opt = torch.optim.Adam([{'params': [model.pre, model.post], 'lr': 1e-4},
                            {'params': list(model.gru.parameters()) + list(model.out.parameters()), 'lr': 3e-3}])
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=[1e-4, 5e-3], total_steps=steps)
    rng = np.random.default_rng(2)
    warm = max(KPRE + KPOST, GWARM + STATEWARM)
    t0 = time.time()
    for step in range(steps):
        xb, yb, wb = [], [], []
        for _ in range(BATCH):
            i = train_idx[rng.integers(len(train_idx))]
            x, y = xs[i], ys[i]
            start = rng.integers(warm, len(x) - SEG)
            xb.append(x[start - warm:start + SEG]); yb.append(y[start:start + SEG])
            wb.append(1.0 / max(1e-6, float(np.mean(y[start:start + SEG] ** 2))))
        xb = torch.tensor(np.array(xb), device=DEV); yb = torch.tensor(np.array(yb), device=DEV)
        wb = torch.tensor(wb, dtype=torch.float32, device=DEV)
        out = model(xb, gru_from=warm - GWARM - STATEWARM, nograd=STATEWARM)[:, warm:]
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
            out = model(torch.tensor(x[None], device=DEV))[0].cpu().numpy()
            res.append((i, o.null_db(y[None], out[None])))
    return res


if __name__ == '__main__':
    case = sys.argv[1]; steps = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
    hidden = int(sys.argv[3]) if len(sys.argv) > 3 else 16
    model, xs, ys, valid, keep = train(case, steps, hidden)
    n = evaluate(model, xs, ys, valid, keep)
    print(case, f'hidden {hidden}', 'validation nulls', ' '.join('%d:%.1f' % v for v in n), 'mean %.1f' % np.mean([v for _, v in n]))
    torch.save(dict(state={k: v.cpu() for k, v in model.state_dict().items()}, hidden=hidden, cell=CELL, layers=RNN_LAYERS),
               os.path.join(NN, case + f'_{CELL}{RNN_LAYERS}x{hidden}' + os.environ.get('TAG', '') + '.pt'))
