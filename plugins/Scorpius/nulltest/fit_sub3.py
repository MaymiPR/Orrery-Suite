"""Sub parallel fitted by nulling a Python model against the reference on program material: path = gain x low-pass
(order n, Butterworth or with a free Q per section) x high-pass (order m), added to the dry signal."""
import itertools
import numpy as np
from scipy.optimize import least_squares, minimize
from scipy.signal import butter, sosfilt
import orion as o
import nullmod as N

FS = 48000
sig = N.signals()
xs = [v if v.ndim == 1 else v[0] for v in sig.values()]
refs = o.render_many([dict(x=x, params={**o.ONLY('sub'), 'Par Sub Level': 1}) for x in xs], cache=True)

def path(p, n, k, x):
    g, fl, fh = p[:3]
    sos = np.concatenate([butter(n, fl, 'low', fs=FS, output='sos'), butter(k, fh, 'high', fs=FS, output='sos')])
    return g * sosfilt(sos, x)

def nulls(p, n, k):
    return [o.null_db(r[:1], (x + path(p, n, k, x))[None]) for x, r in zip(xs, refs)]

if __name__ == '__main__':
    for n, k in itertools.product((4, 5, 6, 7, 8), (4, 6, 8)):
        cost = lambda p: np.mean(nulls(p, n, k))
        s = minimize(cost, [2, 140, 14], method='Nelder-Mead', options=dict(xatol=1e-3, fatol=1e-3, maxiter=300))
        print(f'LP {n} HP {k}: {np.round(s.x, 3).tolist()} nulls', ' '.join('%.1f' % v for v in nulls(s.x, n, k)), flush=True)
