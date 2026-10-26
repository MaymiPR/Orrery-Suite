"""Harmonic EQ, linear part, general form: 1 + k * B(z), B a cascade of free biquads (poles from (fc, Q) pairs,
numerator free), for the bands a plain band-pass does not fit (40 Hz, 100 Hz, 10 kHz)."""
import numpy as np
from scipy.optimize import least_squares
from fit_eq import measured, f, z, FS

def poles(fc, q):
    w = 2*np.pi*fc/FS; a = np.sin(w)/(2*q); c = np.cos(w)
    return np.array([1, -2*c/(1+a), (1-a)/(1+a)])

def model(p, nsec):
    H = np.ones_like(z)
    for s in range(nsec):
        fc, q, b0, b1, b2 = p[5*s:5*s+5]
        A = poles(fc, q)
        H = H * (b0 + b1*z + b2*z**2) / (A[0] + A[1]*z + A[2]*z**2)
    return H

def fit(band, nsec, guess):
    Hm = {g: measured(f'eq_{band}_{g}') for g in (3, 6)}
    def res(p):
        B = model(p[:-2], nsec); r = []
        for g, k in ((3, p[-2]), (6, p[-1])):
            d = 1 + k*B - Hm[g]; r += [d.real, d.imag]
        return np.concatenate(r)
    best = None
    for trial in range(8):
        p0 = []
        for s in range(nsec):
            fc = guess[s]*np.exp(np.random.default_rng(trial+s).normal(0, 0.3)); p0 += [fc, 0.8, 0.1, 0, -0.1]
        p0 += [0.5, 1.2]
        lo = [10, 0.2, -10, -10, -10]*nsec + [0, 0]; hi = [23000, 8, 10, 10, 10]*nsec + [5, 5]
        try: s_ = least_squares(res, p0, bounds=(lo, hi), max_nfev=4000)
        except Exception: continue
        if best is None or s_.cost < best.cost: best = s_
    p = best.x; B = model(p[:-2], nsec)
    err = max(np.max(np.abs(20*np.log10(np.abs(1+k*B)/np.abs(Hm[g])))) for g, k in ((3, p[-2]), (6, p[-1])))
    perr = max(np.max(np.abs(np.degrees(np.angle((1+k*B)/Hm[g])))) for g, k in ((3, p[-2]), (6, p[-1])))
    return p, err, perr

if __name__ == '__main__':
    for band, g in [('40Hz', [35, 60]), ('100Hz', [100, 40]), ('10kHz', [10000, 15000]), ('1kHz', [1000, 500])]:
        for nsec in (1, 2):
            p, e, pe = fit(band, nsec, g)
            print(band, nsec, 'sections: err %.3f dB %.2f deg' % (e, pe), np.round(p, 4).tolist())
