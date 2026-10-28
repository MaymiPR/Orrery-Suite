"""A learned correction to compression's gain (MEASUREMENTS.md 14: what the wet path leaves after our gain and the
fitted IIR is the gain's dynamics). The wet path becomes
    y = IIR(x * g(x) * 10^(c / 20)),   c = WaveNet(x; our gain in dB as an extra input)
with g our gain computer (Compressor.h, prep_comp.gain) and the IIR the plug-in's own (LinearFits.h compWetA / B, as
a long FIR here); c (dB) starts at zero and is learned on capture_nn.py's comp_a / comp_b material (mono: the mid is
the signal). Validated by nulls on the held-out pieces against c = 0. Writes measurements/scorpius/nn/
comp_<m>_gain.pt in train_wn.py's layout (Pre a unit impulse, Post zero, bypass: the network's output alone), for
export_models.py.
    python train_gain.py a|b [steps] [channels] [layers]"""
import os, sys, time
import numpy as np
import torch
import orion as o
from train_wh import fftconv, NN
from prep_comp import gain, iir, TUNE

os.environ.setdefault('KERNEL', '3'); os.environ.setdefault('MIXIN', '1')
import train_wn as T

FS, SEG, BATCH, KIIR = 48000, 4096, 16, 65536
DEV = T.DEV
GSCALE = 0.1          # the gain (dB) as the network's extra input, scaled


def data(m):
    path = os.path.join(NN, f'comp_{m}_g.npz')
    d = np.load(os.path.join(NN, f'comp_{m}.npz'), allow_pickle=True)
    xs, ys, valid = list(d['x']), list(d['y']), list(d['valid'])
    if os.path.exists(path):
        gdb = list(np.load(path, allow_pickle=True)['gdb'])
    else:
        gdb = [(20 * np.log10(gain(x.astype(np.float64), **TUNE[m]))).astype(np.float32) for x in xs]
        np.savez_compressed(path, gdb=np.array(gdb, dtype=object))
    return xs, ys, valid, gdb


if __name__ == '__main__':
    m = sys.argv[1]; steps = int(sys.argv[2]) if len(sys.argv) > 2 else 15000
    ch = int(sys.argv[3]) if len(sys.argv) > 3 else 4; layers = int(sys.argv[4]) if len(sys.argv) > 4 else 10
    xs, ys, valid, gdb = data(m)
    imp = np.zeros(KIIR); imp[0] = 1
    h = torch.tensor(iir('compWet' + m.upper())(imp).astype(np.float32), device=DEV)
    keep = [i for i, x in enumerate(xs) if 20 * np.log10(np.abs(x).max() + 1e-9) <= T.MAXPEAK]
    tr = [i for i in keep if not valid[i]]
    wn = T.WaveNet(ch, layers, 1).to(DEV)
    warm = KIIR + wn.rf
    tr = [i for i in tr if len(xs[i]) > warm + SEG]

    def wet(x, g, c):                       # (batch, n) each
        return fftconv(x * 10 ** ((g + c) / 20), h)

    def corr(x, g):
        return wn(x, (g * GSCALE).unsqueeze(1))

    opt = torch.optim.Adam(wn.parameters(), lr=2e-3)
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=3e-3, total_steps=steps)
    rng = np.random.default_rng(1); t0 = time.time()
    for step in range(steps):
        xb, gb, yb, wb = [], [], [], []
        for _ in range(BATCH):
            i = tr[rng.integers(len(tr))]
            s = rng.integers(warm, len(xs[i]) - SEG)
            xb.append(xs[i][s - warm:s + SEG]); gb.append(gdb[i][s - warm:s + SEG]); yb.append(ys[i][s:s + SEG])
            wb.append(1.0 / max(1e-6, float(np.mean(ys[i][s:s + SEG] ** 2))))
        xb, gb, yb = (torch.tensor(np.array(v), device=DEV) for v in (xb, gb, yb))
        wb = torch.tensor(wb, dtype=torch.float32, device=DEV)
        out = wet(xb, gb, corr(xb, gb))[:, warm:]
        loss = (((out - yb) ** 2).mean(1) * wb).mean()
        opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(wn.parameters(), 1.0); opt.step(); sched.step()
        if step % 500 == 0 or step == steps - 1:
            print(f'comp {m} gain step {step}: loss {10 * np.log10(loss.item()):.1f} dB ({time.time() - t0:.0f} s)', flush=True)
    res = []
    with torch.no_grad():
        for i in keep:
            if not valid[i]: continue
            x = torch.tensor(xs[i][None], device=DEV); g = torch.tensor(gdb[i][None], device=DEV)
            base = wet(x, g, 0 * g)[0].cpu().numpy(); new = wet(x, g, corr(x, g))[0].cpu().numpy()
            res.append((i, o.null_db(ys[i][None], base[None]), o.null_db(ys[i][None], new[None])))
    print(f'comp {m} gain ch {ch} layers {layers} held-out nulls (IIR alone -> corrected):',
          ' '.join('%d:%.1f->%.1f' % r for r in res), 'mean %.1f -> %.1f' % (np.mean([r[1] for r in res]), np.mean([r[2] for r in res])))
    pre = torch.zeros(64); pre[0] = 1
    state = {'pre': pre, 'post': torch.zeros(64), **{'wn.' + k: v.cpu() for k, v in wn.state_dict().items()}}
    torch.save(dict(state=state, ch=ch, layers=layers, dil=wn.dil, feat=[], bypass=True, cond=True, gscale=GSCALE),
               os.path.join(NN, f'comp_{m}_gain' + os.environ.get('TAG', '') + '.pt'))
