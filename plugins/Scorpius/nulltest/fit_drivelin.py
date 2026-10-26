"""Drive, small-signal response per colour (linear.py's impulse responses, hi-pass off): gain x elliptic high-pass
(order n, passband ripple rp, stopband rs, corner fc; scipy's design) x an RBJ bell x an RBJ high shelf, fitted in
magnitude and phase from 12 Hz to 22 kHz.
    python fit_drivelin.py"""
import os, itertools
import numpy as np
from scipy.optimize import least_squares
from scipy.signal import ellip, cheby1, sosfreqz
import orion as o
from fit_eqrbj import rbj

FS = 48000
f = np.geomspace(12, 22000, 700)
z = np.exp(-2j * np.pi * f / FS)

def measured(name):
    h = np.load(os.path.join(o.ROOT, 'measurements', 'scorpius', 'ir', name + '.npy'))[0].astype(np.float64)
    H = np.fft.rfft(h, 1 << 18); fr = np.fft.rfftfreq(1 << 18, 1 / FS)
    return np.interp(f, fr, H.real) + 1j * np.interp(f, fr, H.imag)

def bq(b, a): return (b[0] + b[1] * z + b[2] * z ** 2) / (a[0] + a[1] * z + a[2] * z ** 2)

def hp(kind, n, fc, rp, rs):
    if kind == 'ellip': sos = ellip(n, rp, rs, fc, 'high', fs=FS, output='sos')
    else: sos = cheby1(n, rp, fc, 'high', fs=FS, output='sos')
    return sosfreqz(sos, worN=f, fs=FS)[1]

def model(p, kind, n):
    g, fc, rp, rs, pf, pq, pg, sf, sq, sg = p
    return g * hp(kind, n, fc, rp, rs) * bq(*rbj('peak', pf, pq, pg)) * bq(*rbj('highshelf', sf, sq, sg))

def fit(name, kind, n):
    M = measured(name)
    w = np.where(f < 30, 0.3, 1.0)
    def res(p):
        d = (model(p, kind, n) - M) * w / np.maximum(np.abs(M), 0.05); return np.concatenate([d.real, d.imag])
    best = None
    for fc, pf in itertools.product((40, 50), (2500, 800)):
        try:
            s = least_squares(res, [0.9, fc, 1.5, 30, pf, 0.5, -1.0, 15000, 0.7, 3.0],
                              bounds=([0.1, 20, 0.05, 10, 100, 0.1, -12, 2000, 0.2, -12], [3, 200, 6, 80, 15000, 5, 12, 23000, 3, 12]), max_nfev=3000)
        except Exception: continue
        if best is None or s.cost < best.cost: best = s
    Hm = model(best.x, kind, n); m = f > 30
    return best.x, np.abs(20 * np.log10(np.abs(Hm[m]) / np.abs(M[m]))).max(), np.abs(np.degrees(np.angle(Hm[m] / M[m]))).max()

if __name__ == '__main__':
    for c in 'abc':
        for kind, n in (('ellip', 4), ('ellip', 6), ('cheby', 4), ('cheby', 6)):
            p, e, pe = fit(f'drive_{c}_hp0', kind, n)
            print(f'{c} {kind}{n}: err>30Hz {e:.3f} dB {pe:.2f} deg', np.round(p, 3).tolist(), flush=True)
