"""Grit parallel (a transient shaper, Parallels.h's GritPath) fitted by nulling at level 1 against the reference on
program material and tone bursts (compression off: with it on, the reference's grit listens to the compressor)."""
import numpy as np
from scipy.optimize import minimize
import orion as o
import nullmod as N

FS = 48000
sig = N.signals()
t = np.arange(FS) / FS
bursts = [(np.where(t >= 0.2, 10 ** (lv / 20), 0) * np.sin(2 * np.pi * 1000 * (t - 0.2))).astype(np.float32) for lv in (-30, -18, -12, -6, 0)]
xs = list(sig.values()) + bursts
P = {**o.ONLY('grit'), 'Par Grit Level': 1}
refs = o.render_many([dict(x=x, params=P) for x in xs], cache=True)
KEYS = ['gritFastAttackMs', 'gritFastReleaseMs', 'gritSlowAttackMs', 'gritSlowReleaseMs', 'gritAmount', 'gritMaxDb', 'gritThresholdDb', 'gritRangeDb']
best = [None, 0]

def cost(v):
    if min(v[:6]) <= 0 or v[7] <= 0: return 0.0
    ours = o.render_many([dict(x=x, params=P, tune=dict(zip(KEYS, map(float, v)))) for x in xs], plugin='model')
    n = [o.null_db(r, m) for r, m in zip(refs, ours)]; c = float(np.mean(n))
    if c < best[1]: best[0], best[1] = list(v), c; print('%.2f' % c, np.round(v, 3).tolist(), ' '.join('%.0f' % x for x in n), flush=True)
    return c

if __name__ == '__main__':
    v0 = [0.3, 5.0, 10.0, 30.0, 1.0, 9.0, -40.0, 25.0]
    minimize(cost, v0, method='Nelder-Mead', options=dict(maxiter=400, xatol=0.01, fatol=0.02,
             initial_simplex=[v0] + [list(np.array(v0) + d) for d in np.diag([0.3, 4, 8, 20, 0.5, 4, 10, 10])]))
    print('best', best)
