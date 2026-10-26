"""Harmonic EQ, linear part: each band's small-signal response (linear.py's impulse responses at +3 and +6) as
1 + k * H(f), H a second-order band-pass (or high-pass / low-pass) with unity peak, fitted in magnitude and phase
from 20 Hz to 20 kHz.
    python fit_eq.py"""
import os
import numpy as np
from scipy.optimize import least_squares
import orion as o

IR = os.path.join(o.ROOT, 'measurements', 'scorpius', 'ir')
FS = o.FS
f = np.geomspace(20, 20000, 400)
z = np.exp(-2j * np.pi * f / FS)


def biquad(kind, fc, q):
    w = 2 * np.pi * fc / FS; a = np.sin(w) / (2 * q); c = np.cos(w)
    if kind == 'bp': b = [a, 0, -a]
    elif kind == 'hp': b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]
    elif kind == 'lp': b = [(1 - c) / 2, 1 - c, (1 - c) / 2]
    A = [1 + a, -2 * c, 1 - a]
    return np.array(b) / A[0], np.array(A) / A[0]


def resp(b, a):
    return (b[0] + b[1] * z + b[2] * z ** 2) / (a[0] + a[1] * z + a[2] * z ** 2)


def measured(name):
    h = np.load(os.path.join(IR, name + '.npy'))[0].astype(np.float64)
    H = np.fft.rfft(h, 1 << 18); fr = np.fft.rfftfreq(1 << 18, 1 / FS)
    return np.interp(f, fr, H.real) + 1j * np.interp(f, fr, H.imag)


def fit(band, kind, f0, q0):
    Hm = {g: measured(f'eq_{band}_{g}') for g in (3, 6)}
    def model(p):
        fc, q, k3, k6 = p
        H = resp(*biquad(kind, fc, q))
        return {3: 1 + k3 * H, 6: 1 + k6 * H}
    def res(p):
        m = model(p); r = []
        for g in (3, 6): r += [(m[g] - Hm[g]).real, (m[g] - Hm[g]).imag]
        return np.concatenate(r)
    s = least_squares(res, [f0, q0, 0.5, 1.2], bounds=([10, 0.1, 0, 0], [22000, 10, 5, 5]))
    m = model(s.x)
    err = max(np.max(np.abs(20 * np.log10(np.abs(m[g]) / np.abs(Hm[g])))) for g in (3, 6))
    perr = max(np.max(np.abs(np.degrees(np.angle(m[g] / Hm[g])))) for g in (3, 6))
    return s.x, err, perr


if __name__ == '__main__':
    for band, kinds, f0 in [('40Hz', ('bp', 'lp'), 35), ('100Hz', ('bp',), 100), ('1kHz', ('bp',), 1000),
                            ('3kHz', ('bp',), 3000), ('10kHz', ('bp', 'hp'), 12000)]:
        for kind in kinds:
            p, e, pe = fit(band, kind, f0, 0.8)
            print(f'{band:6s} {kind}: fc {p[0]:8.2f} Q {p[1]:.3f} k(+3) {p[2]:.4f} k(+6) {p[3]:.4f}  '
                  f'(20 log(1+k): {20*np.log10(1+p[2]):.2f} / {20*np.log10(1+p[3]):.2f} dB)  max err {e:.3f} dB {pe:.2f} deg')
