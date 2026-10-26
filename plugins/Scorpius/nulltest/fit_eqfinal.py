"""Harmonic EQ bands, final form: an RBJ filter (bells; the 10 kHz band a high shelf) whose boost (H - 1) passes a
20 Hz Butterworth high-pass before it is added. Fitted per band: f0, Q, the gain at +3 and +6."""
import numpy as np
from scipy.optimize import least_squares
from fit_eqbands import measured
from fit_eqrbj import rbj, H
from fit_eqlow import hp

def fit(band, kind, f0, hpf=20.0, hpq=0.7071):
    M = {g: measured(f'eq_{band}_{g}') for g in (3, 6)}
    def model(p, gdb):
        return 1 + (H(*rbj(kind, p[0], p[1], gdb)) - 1) * (H(*hp(hpf, hpq, 'hp')) if hpf else 1)
    def res(p):
        r = []
        for g, gdb in ((3, p[2]), (6, p[3])):
            d = model(p, gdb) - M[g]; r += [d.real, d.imag]
        return np.concatenate(r)
    best = None
    for q0 in (0.5, 0.7, 1.0):
        s = least_squares(res, [f0, q0, 3.6, 7.1], bounds=([5, .1, 0, 0], [23000, 10, 20, 20]))
        if best is None or s.cost < best.cost: best = s
    p = best.x
    e = max(np.max(np.abs(20*np.log10(np.abs(model(p, gdb))/np.abs(M[g])))) for g, gdb in ((3, p[2]), (6, p[3])))
    pe = max(np.max(np.abs(np.degrees(np.angle(model(p, gdb)/M[g])))) for g, gdb in ((3, p[2]), (6, p[3])))
    return p, e, pe

if __name__ == '__main__':
    for band, kind, f0 in [('40Hz', 'peak', 40), ('100Hz', 'peak', 100), ('1kHz', 'peak', 1000), ('3kHz', 'peak', 3000), ('10kHz', 'highshelf', 10000)]:
        for hpf in (20.0, 0):
            p, e, pe = fit(band, kind, f0, hpf)
            print(f'{band:6s} {kind:9s} hp {hpf:4.0f}: f0 {p[0]:9.3f} Q {p[1]:.4f} G3 {p[2]:.3f} G6 {p[3]:.3f}  err {e:.3f} dB {pe:.2f} deg')
