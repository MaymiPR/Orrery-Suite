"""Learns a module of the reference with a small WaveNet (dilated causal convolutions, gated) between two learned linear
filters (PyTorch; on the GPU when there is one):
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
KPRE, KPOST, SEG, BATCH = int(os.environ.get("KPRE", 64)), 8192, 4096, 16
STACK = int(os.environ.get("STACK", 12))
# FEAT: time constants (ms) of slow envelopes of the input, one-pole averages of |x|, given to the network as extra
# inputs (memory far beyond its receptive field, cheap to compute in the plug-in), e.g. FEAT=10,50,200,1000
FEAT = [float(v) for v in os.environ.get("FEAT", "").split(",") if v]
# COND: captures at several settings of a control, "case:value,...": the value is an extra (constant) input, and
# each target the fully wet equivalent x + (y - x) / weight(value) (harmonics' amount: Scorpius mixes the model's
# output in by the weight, so the model learns what the amount changes beyond the mix)
COND = [(c.split(':')[0], float(c.split(':')[1])) for c in os.environ.get("COND", "").split(",") if c]
EQUIV = float(os.environ.get("EQUIV", 0))   # a capture at amount EQUIV: target its fully wet equivalent (no extra input)
BYPASS = os.environ.get("BYPASS") == "1"
# the layer's shape (NAM's WaveNets: kernel 3, the input mixed into every layer, tanh without a gate)
KERNEL = int(os.environ.get("KERNEL", 2)); MIXIN = os.environ.get("MIXIN") == "1"; GATED = os.environ.get("GATED", "1") == "1"
LOSSP = float(os.environ.get("LOSSP", 1.0))   # each slice's loss / its output power ^ LOSSP (below 1: loud slices count more)   # y = Post(u) + WaveNet(u) instead of Post(u + WaveNet(u))
MAXPEAK = 4.0          # dBFS: training and validation pieces up to this peak
torch.set_num_threads(int(os.environ.get("THREADS", max(1, os.cpu_count() // 2))))
DEV = torch.device("cuda" if torch.cuda.is_available() else "cpu")
torch.backends.cudnn.allow_tf32 = torch.backends.cuda.matmul.allow_tf32 = False    # (full float precision, as on the CPU)


def amount_weight(a):
    """harmonics' measured mix weights (LearnedModules.h Harmonics::weight)"""
    return float(np.interp(a, [0, 0.25, 0.5, 0.75, 1], [0, 0.237, 0.483, 0.738, 1]))


def load_cond():
    xs, ys, valid, conds = [], [], [], []
    for c, v in COND:
        x_, y_, val_ = load(c); w = amount_weight(v)
        xs += x_; ys += [x + (y - x) / w for x, y in zip(x_, y_)]; valid += val_; conds += [v] * len(x_)
    return xs, ys, valid, conds


def features(x):
    """the slow envelopes of x (FEAT), (len(FEAT), n), float32"""
    import scipy.signal as sg
    out = []
    for ms in FEAT:
        a = np.exp(-1.0 / (ms * 0.001 * FS))
        out.append(sg.lfilter([1 - a], [1, -a], np.abs(x.astype(np.float64))))
    return np.array(out, np.float32).reshape(len(FEAT), len(x))


class WaveNet(torch.nn.Module):
    def __init__(self, ch=8, layers=10, nfeat=0):
        super().__init__()
        self.dil = [2 ** (i % STACK) for i in range(layers)]     # (STACK: dilations 1 ... 2^(STACK-1), repeated)
        self.inp = torch.nn.Conv1d(1 + nfeat, ch, 1)
        g = 2 if GATED else 1
        self.conv = torch.nn.ModuleList([torch.nn.Conv1d(ch, g * ch, KERNEL, dilation=d) for d in self.dil])
        self.mix = torch.nn.ModuleList([torch.nn.Conv1d(1, g * ch, 1, bias=False) for _ in self.dil]) if MIXIN else None
        self.res = torch.nn.ModuleList([torch.nn.Conv1d(ch, ch, 1) for _ in self.dil])
        self.skip = torch.nn.ModuleList([torch.nn.Conv1d(ch, ch, 1) for _ in self.dil])
        self.out = torch.nn.Conv1d(ch, 1, 1)
        with torch.no_grad(): self.out.weight.zero_(); self.out.bias.zero_()
        self.rf = (KERNEL - 1) * sum(self.dil) + 1

    def forward(self, u, f=None):             # (batch, n), features (batch, nfeat, n) -> (batch, n), causal
        h = self.inp(u.unsqueeze(1) if f is None else torch.cat([u.unsqueeze(1), f], 1)); s = 0
        for i, (d, conv, res, skip) in enumerate(zip(self.dil, self.conv, self.res, self.skip)):
            z = conv(torch.nn.functional.pad(h, ((KERNEL - 1) * d, 0)))
            if MIXIN: z = z + self.mix[i](u.unsqueeze(1))
            if GATED:
                a, b = z.chunk(2, 1)
                g = torch.tanh(a) * torch.sigmoid(b)
            else:
                g = torch.tanh(z)
            h = h + res(g); s = s + skip(g)
        return self.out(torch.relu(s)).squeeze(1)


class Model(torch.nn.Module):
    def __init__(self, post_init, ch, layers):
        super().__init__()
        pre = torch.zeros(KPRE); pre[0] = 1.0
        self.pre = torch.nn.Parameter(pre)
        self.post = torch.nn.Parameter(torch.tensor(post_init[:KPOST], dtype=torch.float32))
        self.wn = WaveNet(ch, layers, 1 if COND else len(FEAT))

    def forward(self, x, f=None):
        u = fftconv(x, self.pre)
        if BYPASS:                           # the network's output beside the long filter, not through it
            return fftconv(u, self.post) + self.wn(u, f)
        return fftconv(u + self.wn(u, f), self.post)


def train(case, steps, irname, ch, layers):
    if COND: xs, ys, valid, conds = load_cond()
    else: xs, ys, valid = load(case)
    for extra in [c for c in os.environ.get("UNION", "").split(",") if c]:     # (UNION=more,cases: their pieces too)
        x_, y_, v_ = load(extra); xs += x_; ys += y_; valid += v_
    if EQUIV: ys = [x + (y - x) / amount_weight(EQUIV) for x, y in zip(xs, ys)]
    keep = [i for i, x in enumerate(xs) if 20 * np.log10(np.abs(x).max() + 1e-9) <= MAXPEAK]
    if irname is None:                  # (grit: steady signals pass unchanged, so the filters start as the identity)
        h = np.zeros(KPOST, np.float32); h[0] = 1.0
    else:
        h = np.load(os.path.join(IR, irname + '.npy'))[0].astype(np.float32)
    model = Model(h, ch, layers).to(DEV)
    train_idx = [i for i in keep if not valid[i]]
    if os.environ.get('PIECES'):         # (PIECES=3: train on those pieces only, held out or not: a capacity test)
        train_idx = [int(v) for v in os.environ['PIECES'].split(',')]
    if os.environ.get('KINDS'):          # (KINDS=track,drums: train on those kinds of material only)
        kinds = np.load(os.path.join(NN, case + '.npz'), allow_pickle=True)['kind']
        train_idx = [i for i in train_idx if kinds[i] in os.environ['KINDS'].split(',')]
    opt = torch.optim.Adam([{'params': [model.pre, model.post], 'lr': 1e-4}, {'params': model.wn.parameters(), 'lr': 2e-3}])
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=[1e-4, 3e-3], total_steps=steps)
    rng = np.random.default_rng(1)
    warm = KPRE + KPOST + model.wn.rf
    train_idx = [i for i in train_idx if len(xs[i]) > warm + SEG]   # (long memories need long pieces)
    F = [np.full((1, len(x)), c, np.float32) for x, c in zip(xs, conds)] if COND else ([features(x) for x in xs] if FEAT else None)
    t0 = time.time()
    for step in range(steps):
        xb, yb, wb, fb = [], [], [], []
        for _ in range(BATCH):
            i = train_idx[rng.integers(len(train_idx))]
            x, y = xs[i], ys[i]
            start = rng.integers(warm, len(x) - SEG)
            xb.append(x[start - warm:start + SEG]); yb.append(y[start:start + SEG])
            if F is not None: fb.append(F[i][:, start - warm:start + SEG])
            wb.append(1.0 / max(1e-6, float(np.mean(y[start:start + SEG] ** 2))) ** LOSSP)
        xb = torch.tensor(np.array(xb), device=DEV); yb = torch.tensor(np.array(yb), device=DEV)
        wb = torch.tensor(wb, dtype=torch.float32, device=DEV)
        fb = torch.tensor(np.array(fb), device=DEV) if F is not None else None
        out = model(xb, fb)[:, warm:]
        loss = (((out - yb) ** 2).mean(1) * wb).mean()
        opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0); opt.step(); sched.step()
        if step % 500 == 0 or step == steps - 1:
            print(f'{case} step {step}: loss {10*np.log10(loss.item()):.1f} dB ({time.time()-t0:.0f} s)', flush=True)
    return model, xs, ys, valid, keep, F


def evaluate(model, xs, ys, valid, keep, all_=False, F=None):
    res = []
    with torch.no_grad():
        for i, (x, y, v) in enumerate(zip(xs, ys, valid)):
            if i not in keep or (not v and not all_): continue
            f = torch.tensor(F[i][None], device=DEV) if F is not None else None
            out = model(torch.tensor(x[None], device=DEV), f)[0].cpu().numpy()
            res.append((i, o.null_db(y[None], out[None])))
    return res


def irname_of(case):
    if case == 'lift': return 'lift_1.0'
    if case == 'grit' or case.startswith('harm'): return None
    return f'drive_{case[6]}_hp{1 if case.endswith("_hp") else 0}'


if __name__ == '__main__':
    case = sys.argv[1]; steps = int(sys.argv[2]) if len(sys.argv) > 2 else 6000
    ch = int(sys.argv[3]) if len(sys.argv) > 3 else 8; layers = int(sys.argv[4]) if len(sys.argv) > 4 else 10
    model, xs, ys, valid, keep, F = train(case, steps, irname_of(case), ch, layers)
    n = evaluate(model, xs, ys, valid, keep, F=F)
    print(case, f'ch {ch} layers {layers}', 'validation nulls', ' '.join('%d:%.1f' % v for v in n), 'mean %.1f' % np.mean([v for _, v in n]))
    torch.save(dict(state={k: v.cpu() for k, v in model.state_dict().items()}, ch=ch, layers=layers, dil=model.wn.dil, feat=FEAT, bypass=BYPASS, cond=bool(COND)), os.path.join(NN, case + '_wn' + os.environ.get('TAG', '') + '.pt'))
