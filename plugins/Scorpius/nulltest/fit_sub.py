"""Sub parallel: its path (linear.py's impulse response at level 1, less the dry signal) as gain x low-pass x high-pass,
each a cascade of second-order sections with free Q (Butterworth-like), fitted in magnitude and phase."""
import itertools, os
import numpy as np
from scipy.optimize import least_squares
import orion as o
from fit_iir import f, z, response, FS

def sec(kind, fc, q):
    w = 2*np.pi*fc/FS; al = np.sin(w)/(2*q); c = np.cos(w)
    b = [(1-c)/2, 1-c, (1-c)/2] if kind == 'lp' else [(1+c)/2, -(1+c), (1+c)/2]
    a = [1+al, -2*c, 1-al]
    return np.array(b)/a[0], np.array(a)/a[0]

def model(p, nl, nh, sep):
    g = p[0]; H = g*np.ones_like(z); k = 1
    for kind, n in (('lp', nl), ('hp', nh)):
        for i in range(n):
            fc = p[k] if (not sep or i == 0) else p[k]
            q = p[k+1]
            b, a = sec(kind, fc, q); H = H*(b[0]+b[1]*z+b[2]*z**2)/(a[0]+a[1]*z+a[2]*z**2); k += 2
    return H

def fit(h, nl, nh):
    T = response(h); m = np.abs(T) > np.abs(T).max()*1e-2
    def res(p):
        d = (model(p, nl, nh, True) - T)[m]/np.abs(T[m]); return np.concatenate([d.real, d.imag])
    best = None
    for t in range(10):
        rng = np.random.default_rng(t)
        p0 = [2.0] + sum([[150*np.exp(rng.normal(0, .2)), .5+rng.random()] for _ in range(nl)], []) + sum([[18*np.exp(rng.normal(0, .2)), .5+rng.random()] for _ in range(nh)], [])
        lo = [0] + [20, .2]*(nl+nh); hi = [10] + [2000, 10]*nl + [100, 10]*nh
        try: s = least_squares(res, p0, bounds=(lo, hi), max_nfev=4000)
        except Exception: continue
        if best is None or s.cost < best.cost: best = s
    H = model(best.x, nl, nh, True)
    e = np.abs(20*np.log10(np.abs(H[m])/np.abs(T[m]))).max(); pe = np.abs(np.degrees(np.angle(H[m]/T[m]))).max()
    return best.x, e, pe

if __name__ == '__main__':
    h = np.load(os.path.join(o.ROOT, 'measurements', 'scorpius', 'ir', 'par_sub_1.0.npy'))[0].astype(float); h[0] -= 1
    for nl, nh in itertools.product((2, 3, 4), (1, 2, 3)):
        p, e, pe = fit(h, nl, nh)
        print(f'LP {2*nl}th, HP {2*nh}th: err {e:.3f} dB {pe:.1f} deg', np.round(p, 3).tolist(), flush=True)
