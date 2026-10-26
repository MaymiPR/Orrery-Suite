"""Compression: the constants of Source/dsp/Compressor.h per mode fitted by nulling our processor (ScRender) against
the reference at amount 1 on program material and tone steps (Nelder-Mead on the mean null).
    python fit_comp.py [a|b]"""
import sys
import numpy as np
from scipy.optimize import minimize
import orion as o
import nullmod as N

FS = 48000
mode = sys.argv[1] if len(sys.argv) > 1 else 'b'
M = 'A' if mode == 'a' else 'B'
sig = N.signals()
t = np.arange(3 * FS) / FS
steps = []
for hi in (-6, 0, 6):
    lev = np.where((t >= 1) & (t < 2), 10 ** (hi / 20), 10 ** (-30 / 20)); steps.append((lev * np.sin(2 * np.pi * 1000 * t)).astype(np.float32))
xs = list(sig.values()) + steps
P = {**o.ONLY('comp'), 'Comp Amount': 1}
H = {'ids.compression.mode': f'Mode {M}'}
refs = o.render_many([dict(x=x, params=P, hidden=H) for x in xs], cache=True)
KEYS = ['thresh', 'ratio', 'knee', 'attack', 'release', 'rms', 'gAttack', 'gRelease']
best = [None, 0]

def nulls(v):
    tune = {f'comp{M}.{k}': float(x) for k, x in zip(KEYS, v)}
    ours = o.render_many([dict(x=x, params=P, hidden=H, tune=tune) for x in xs], plugin='model')
    return [o.null_db(r, m) for r, m in zip(refs, ours)]

def cost(v):
    if v[1] < 1 or v[2] < 0 or v[3] <= 0.01 or v[4] <= 0.1 or not 0 <= v[5] <= 1 or v[6] < 0 or v[7] < 0: return 0.0
    n = nulls(v); c = float(np.mean(n))
    if c < best[1]: best[0], best[1] = list(v), c; print('%.2f' % c, np.round(v, 3).tolist(), ' '.join('%.1f' % x for x in n), flush=True)
    return c

if __name__ == '__main__':
    v0 = [-12.828, 4.983, 7.225, 1.03, 20.7, 0.04, 0.0, 0.0]
    minimize(cost, v0, method='Nelder-Mead', options=dict(maxiter=300, xatol=1e-3, fatol=0.02,
             initial_simplex=[v0] + [list(np.array(v0) + d) for d in np.diag([1.0, 0.5, 2.0, 0.5, 5.0, 0.1, 0.5, 5.0])]))
    print('best', best)
