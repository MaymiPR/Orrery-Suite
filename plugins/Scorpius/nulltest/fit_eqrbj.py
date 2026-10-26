"""Harmonic EQ bands as RBJ-cookbook filters (peaking, low / high shelf), their gain in dB from the knob: fitted to
linear.py's impulse responses at +3 and +6 (f0, Q, the dB gain at each)."""
import numpy as np
from scipy.optimize import least_squares
from fit_eqbands import measured, f, z, FS

def rbj(kind, f0, q, gdb):
    A = 10 ** (gdb / 40); w = 2*np.pi*f0/FS; al = np.sin(w)/(2*q); c = np.cos(w)
    if kind == 'peak':
        b = [1+al*A, -2*c, 1-al*A]; a = [1+al/A, -2*c, 1-al/A]
    elif kind == 'lowshelf':
        s = 2*np.sqrt(A)*al
        b = [A*((A+1)-(A-1)*c+s), 2*A*((A-1)-(A+1)*c), A*((A+1)-(A-1)*c-s)]
        a = [(A+1)+(A-1)*c+s, -2*((A-1)+(A+1)*c), (A+1)+(A-1)*c-s]
    elif kind == 'highshelf':
        s = 2*np.sqrt(A)*al
        b = [A*((A+1)+(A-1)*c+s), -2*A*((A-1)+(A+1)*c), A*((A+1)+(A-1)*c-s)]
        a = [(A+1)-(A-1)*c+s, 2*((A-1)-(A+1)*c), (A+1)-(A-1)*c-s]
    b = np.array(b)/a[0]; a = np.array(a)/a[0]
    return b, a

def H(b, a): return (b[0]+b[1]*z+b[2]*z**2)/(a[0]+a[1]*z+a[2]*z**2)

def fit(band, kind, f0):
    M = {g: measured(f'eq_{band}_{g}') for g in (3, 6)}
    def res(p):
        r = []
        for g, gdb in ((3, p[2]), (6, p[3])):
            d = H(*rbj(kind, p[0], p[1], gdb)) - M[g]; r += [d.real, d.imag]
        return np.concatenate(r)
    best=None
    for q0 in (0.4, 0.7, 1.0, 1.5):
        s = least_squares(res, [f0, q0, 3.6, 7.1], bounds=([10, 0.1, 0, 0], [23000, 10, 20, 20]))
        if best is None or s.cost < best.cost: best = s
    p = best.x
    err = max(np.max(np.abs(20*np.log10(np.abs(H(*rbj(kind, p[0], p[1], gdb)))/np.abs(M[g])))) for g, gdb in ((3, p[2]), (6, p[3])))
    return p, err

for band, kinds, f0 in [('40Hz', ('peak', 'lowshelf'), 35), ('100Hz', ('peak',), 100), ('1kHz', ('peak',), 1000), ('3kHz', ('peak',), 3000), ('10kHz', ('peak', 'highshelf'), 12000)]:
    for k in kinds:
        p, e = fit(band, k, f0)
        print(f'{band:6s} {k:9s} f0 {p[0]:9.2f} Q {p[1]:.4f} G(+3) {p[2]:.3f} G(+6) {p[3]:.3f} dB   max err {e:.3f} dB')
