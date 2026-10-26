"""Harmonic EQ: the soft clip's ceiling and knee (and per-band ceiling scales) fitted by nulling our processor against
the reference on loud program material (nullmod.py's signals), all bands at +6 one at a time."""
import numpy as np
from scipy.optimize import minimize
import orion as o
import nullmod as N

sig = N.signals()
loud = {k: v for k, v in sig.items() if '-' not in k}
CASES = ['eq 40 6', 'eq 100 6', 'eq 1k 6', 'eq 3k 6', 'eq 10k 6']
jobs = [dict(x=x, **N.CASES[c]) for c in CASES for x in loud.values()]
ref = o.render_many(jobs, cache=True)

def cost(tune):
    ours = o.render_many([dict(j, tune=tune) for j in jobs], plugin='model')
    return np.mean([o.null_db(r, m) for r, m in zip(ref, ours)])

def f(v):
    t = {'eqCeiling': v[0], 'eqKnee': v[1]}
    c = cost(t); print('%.4f %.3f -> %.2f' % (v[0], v[1], c), flush=True); return c

if __name__ == '__main__':
    s = minimize(f, [0.4624, 2.094], method='Nelder-Mead', options=dict(xatol=1e-3, fatol=0.05, maxiter=40))
    print('best', s.x, s.fun)
