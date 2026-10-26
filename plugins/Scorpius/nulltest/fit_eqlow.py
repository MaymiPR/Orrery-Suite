"""The 40 Hz and 100 Hz bands: an RBJ peaking filter whose boost (H - 1) passes a fixed filter (high-pass or
low-pass, second order, or both) before it is added."""
import numpy as np
from scipy.optimize import least_squares
from fit_eqbands import measured, f, z, FS
from fit_eqrbj import rbj, H

def hp(fc, q, kind):
    w = 2*np.pi*fc/FS; al = np.sin(w)/(2*q); c = np.cos(w)
    b = [(1+c)/2, -(1+c), (1+c)/2] if kind == 'hp' else [(1-c)/2, 1-c, (1-c)/2]
    a = [1+al, -2*c, 1-al]
    return np.array(b)/a[0], np.array(a)/a[0]

def fit(band, f0, chain):
    M = {g: measured(f'eq_{band}_{g}') for g in (3, 6)}
    def model(p, gdb):
        Hb = H(*rbj('peak', p[0], p[1], gdb)) - 1
        for i, kind in enumerate(chain): Hb = Hb * H(*hp(p[4+2*i], p[5+2*i], kind))
        return 1 + Hb
    def res(p):
        r = []
        for g, gdb in ((3, p[2]), (6, p[3])):
            d = model(p, gdb) - M[g]; r += [d.real, d.imag]
        return np.concatenate(r)
    best = None
    for t in range(10):
        rng = np.random.default_rng(t)
        p0 = [f0*np.exp(rng.normal(0, .2)), 0.5+rng.random(), 3.6, 7.1]
        lo = [10, .1, 0, 0]; hi = [2000, 10, 20, 20]
        for kind in chain:
            p0 += [(25 if kind == 'hp' else 300)*np.exp(rng.normal(0, .3)), 0.5+rng.random()]; lo += [5, .2]; hi += [5000, 5]
        s = least_squares(res, p0, bounds=(lo, hi), max_nfev=5000)
        if best is None or s.cost < best.cost: best = s
    p = best.x
    err = max(np.max(np.abs(20*np.log10(np.abs(model(p, gdb))/np.abs(M[g])))) for g, gdb in ((3, p[2]), (6, p[3])))
    perr = max(np.max(np.abs(np.degrees(np.angle(model(p, gdb)/M[g])))) for g, gdb in ((3, p[2]), (6, p[3])))
    return p, err, perr

for band, f0 in (('40Hz', 35), ('100Hz', 100)):
    for chain in (('hp',), ('lp',), ('hp', 'lp')):
        p, e, pe = fit(band, f0, chain)
        print(band, chain, 'err %.3f dB %.2f deg' % (e, pe), np.round(p, 4).tolist())
