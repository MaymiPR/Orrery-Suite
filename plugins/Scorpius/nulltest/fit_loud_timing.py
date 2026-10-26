"""The loud parallel's detector timing (its gain table from fit_loud.py kept): attack, release and RMS share fitted
on level steps, drums and the track, the error measured on the parallel's contribution (MEASUREMENTS.md 14).
    python fit_loud_timing.py [param=start ...]      params: loudAttackMs loudReleaseMs loudRms"""
import sys
import numpy as np
from scipy.optimize import minimize
import orion as o
import nullmod as N

FS = o.FS
L = {**o.NONE, 'Par Loud On/Off': 1, 'Par Loud Level': 1.0}
start = dict(a.split('=') for a in sys.argv[1:]) or {'loudAttackMs': '0.6695', 'loudReleaseMs': '12.896', 'loudRms': '0.0096'}
NAMES = list(start)
seg = int(0.25 * FS)
x = np.concatenate([10 ** (db / 20) * np.sin(2 * np.pi * 1000 * np.arange(seg) / FS) for db in (-40, -10, -40, 0, -30, -6, -50, -20)])
X = {'steps': np.stack([x, x]).astype(np.float32)}
X.update({k: (np.stack([v, v]) if v.ndim == 1 else v).astype(np.float32) for k, v in N.signals().items()})
jobs = [dict(x=v, params=L) for v in X.values()]
R = o.render_many(jobs, 'ref', cache=True)
C = [r - v for r, v in zip(R, X.values())]


def errs(v):
    Y = o.render_many([dict(j, tune={k: float(abs(q)) for k, q in zip(NAMES, v)}) for j in jobs], 'model')
    out = []
    for c, y, xx in zip(C, Y, X.values()):
        d = (y - xx)[:, 4800:]; cc = c[:, 4800:]
        out.append(10 * np.log10(np.sum((cc - d) ** 2) / np.sum(cc ** 2)))
    return out


if __name__ == '__main__':
    v0 = [float(v) for v in start.values()]
    print(list(X)); print('now', np.round(errs(v0), 1), flush=True)
    r = minimize(lambda v: float(np.mean(errs(v))), v0, method='Nelder-Mead', options=dict(maxiter=80, xatol=1e-3, fatol=1e-3))
    print('fit', dict(zip(NAMES, np.round(np.abs(r.x), 4))), np.round(errs(r.x), 1), flush=True)
