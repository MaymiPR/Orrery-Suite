"""The interpolator probe (liminterp.py) on a fine frequency grid, the reference's readings against limpy.py's for
interpolator designs (no rendering of ours: the port is the plug-in's limiter).

    python liminterp_py.py"""
import numpy as np
import tgp, limpy, liminterp as L

FREQS = list(range(500, 23001, 250))
PHASES = (0.0, 0.5)


def probe():
    seg = int(0.25 * L.FS); n = L.PRE + seg * len(FREQS) * len(PHASES)
    x = np.zeros(n); plan = []
    for i, (f, ph) in enumerate([(f, p) for f in FREQS for p in PHASES]):
        a = L.PRE + i * seg; tt = np.arange(seg) / L.FS
        x[a:a + seg] = L.A * np.sin(2 * np.pi * f * tt + ph * np.pi); plan.append((f, ph, a))
    return x, plan, seg


def readings(x, y, plan, seg, lag=0):
    out = []
    for f, ph, a in plan:
        s = slice(a + seg // 2, a + seg - 200)
        e = np.exp(-2j * np.pi * f * np.arange(s.stop - s.start) / L.FS)
        xs = x[s.start - lag:s.stop - lag]
        out.append(L.TRIM / (abs(np.dot(y[s], e)) / abs(np.dot(xs, e))) / L.A)
    return np.array(out)


def ours(x, plan, seg):
    e = limpy.env(np.stack([x, x]))
    return readings(x, x * L.TRIM / e, plan, seg)


x, plan, seg = probe()
REF = readings(x, tgp.render_many([dict(x=x.astype(np.float32), norm=tgp.LIM_ONLY, params={'Limiter Input Gain': 0})], 'ref', cache=True)[0][0], plan, seg)

if __name__ == '__main__':
    o = ours(x, plan, seg)
    m = tgp.render_many([dict(x=x.astype(np.float32), norm=tgp.LIM_ONLY, params={'Limiter Input Gain': 0})], 'model')[0][0]
    pm = readings(x, m, plan, seg)
    print('port vs plug-in max', np.max(np.abs(o - pm)))
    for (f, ph, a), r, v in zip(plan, REF, o):
        if ph == 0: print(f'{f:6d}  ref {r:.5f}  ours {v:.5f}  {v - r:+.5f}')
