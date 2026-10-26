"""Loud parallel: the gain table and detector of Parallels.h's LoudPath fitted by nulling (level 1) against the
reference on program material and tone steps."""
import numpy as np
from scipy.optimize import minimize
import orion as o
import nullmod as N

FS = 48000
sig = N.signals()
t = np.arange(3 * FS) / FS
steps = []
for hi, lo in ((-6, -40), (-20, -60), (0, -30)):
    lev = np.where((t >= 1) & (t < 2), 10 ** (hi / 20), 10 ** (lo / 20)); steps.append((lev * np.sin(2 * np.pi * 1000 * t)).astype(np.float32))
tones = [o.tone(1000, 1.0, lv) for lv in range(-66, 13, 6)]
xs = list(sig.values()) + steps + tones
P = {**o.ONLY('loud'), 'Par Loud Level': 1}
refs = o.render_many([dict(x=x, params=P) for x in xs], cache=True)
curve0 = [7.8, 7.2, 6.6, 5.8, 5.1, 4.5, 3.9, 3.4, 2.9, 2.5, 1.9, 1.0, 0.5, 0.3, 0.2]
best = [None, 0]

def tune_of(v):
    t_ = {f'loud{k}': float(v[k]) for k in range(15)}
    t_.update(loudAttackMs=float(v[15]), loudReleaseMs=float(v[16]), loudRms=float(v[17]))
    return t_

def cost(v):
    if v[15] <= 0.01 or v[16] <= 0.1 or not 0 <= v[17] <= 1: return 0.0
    ours = o.render_many([dict(x=x, params=P, tune=tune_of(v)) for x in xs], plugin='model')
    n = [o.null_db(r, m) for r, m in zip(refs, ours)]; c = float(np.mean(n))
    if c < best[1]: best[0], best[1] = list(v), c; print('%.2f' % c, np.round(v, 3).tolist(), ' '.join('%.0f' % x for x in n), flush=True)
    return c

if __name__ == '__main__':
    v0 = curve0 + [1.0, 20.0, 0.0]
    minimize(cost, v0, method='Powell', options=dict(maxiter=4, xtol=1e-2, ftol=1e-3))
    print('best', np.round(best[0], 4).tolist(), best[1])
