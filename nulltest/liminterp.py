"""The limiter's inter-sample level estimate against frequency: a steady sine above the ceiling is limited to
gain = trim / level, so level / amplitude = how far the estimate reads the sine's true peak (1 = exact; a sampled
sine's own sample peaks read lower, more so near Nyquist). Reference against ours, at several phases per frequency.

    python liminterp.py"""
import numpy as np
import tgp

FS = tgp.FS; PRE = 2 * FS; TRIM = 10 ** (-0.1 / 20)
A = 1.5


def run(freqs, phases=(0.0, 0.25, 0.5, 0.75), tune=None):
    seg = int(0.4 * FS)
    n = PRE + seg * len(freqs) * len(phases); t = np.arange(n) / FS
    x = np.zeros(n)
    plan = []
    for i, (f, ph) in enumerate([(f, p) for f in freqs for p in phases]):
        a = PRE + i * seg
        tt = np.arange(seg) / FS
        x[a:a + seg] = A * np.sin(2 * np.pi * f * tt + ph * np.pi)
        plan.append((f, ph, a))
    j = dict(x=x.astype(np.float32), norm=tgp.LIM_ONLY, params={'Limiter Input Gain': 0})
    r = tgp.render_many([j], 'ref', cache=True)[0][0]
    o = tgp.render_many([dict(j, tune=tune or {})], 'model')[0][0]
    out = []
    for f, ph, a in plan:
        s = slice(a + seg // 2, a + seg - 200)
        e = np.exp(-2j * np.pi * f * np.arange(s.stop - s.start) / FS)
        ain = abs(np.dot(x[s], e))
        gr = abs(np.dot(r[s], e)) / ain; go = abs(np.dot(o[s], e)) / ain        # the fundamental's gain
        out.append((f, ph, TRIM / gr, TRIM / go))
    return out


if __name__ == '__main__':
    freqs = [1000, 3000, 6000, 9000, 11000, 12000, 13000, 15000, 17000, 19000, 21000]
    for f, ph, lr, lo in run(freqs):
        print(f'{f:6d} Hz phase {ph:.2f}: level / amplitude  ref {lr / A:.5f}  ours {lo / A:.5f}   diff {(lo - lr) / A:+.5f}')
