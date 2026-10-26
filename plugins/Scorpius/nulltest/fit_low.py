"""Low parallel (a subharmonic synthesizer, Parallels.h's LowPath): its filters, gain and limit fitted by nulling at
level 1 against the reference on program material and bass tones."""
import numpy as np
from scipy.optimize import minimize
import orion as o
import nullmod as N

sig = N.signals()
tones = [o.tone(f, 2.0, lv) for f in (40, 60, 90) for lv in (-20, -6, 0)]
xs = list(sig.values()) + tones
P = {**o.ONLY('low'), 'Par Low Level': 1}
refs = o.render_many([dict(x=x, params=P) for x in xs], cache=True)
KEYS = ['lowInHz', 'lowOutHz', 'lowHpHz', 'lowGain', 'lowCeiling', 'lowSquare']
best = [None, 0]

def cost(v, orders):
    if min(v[:3]) <= 0 or v[4] <= 0: return 0.0
    tune = dict(zip(KEYS, map(float, v))); tune.update(lowInOrder=orders[0], lowOutOrder=orders[1])
    ours = o.render_many([dict(x=x, params=P, tune=tune) for x in xs], plugin='model')
    n = [o.null_db(r, m) for r, m in zip(refs, ours)]; c = float(np.mean(n))
    if c < best[1]: best[0], best[1] = (list(v), orders), c; print('%.2f' % c, orders, np.round(v, 3).tolist(), ' '.join('%.0f' % x for x in n), flush=True)
    return c

if __name__ == '__main__':
    for orders in ((2, 6), (2, 8)):
        minimize(lambda v: cost(v, orders), [150, 55, 5, 1.0, 0.8, 0.5], method='Nelder-Mead', options=dict(maxiter=120, xatol=0.01, fatol=0.05))
    print('best', best)
