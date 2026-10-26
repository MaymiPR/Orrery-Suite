"""Harmonics, linear part at amount 1 (linear.py's impulse responses at 80 and 50 Hz): an RBJ filter (peaking or low
shelf) whose boost passes a Butterworth high-pass (order 2 or 4), fitted in magnitude and phase."""
import os, itertools
import numpy as np
from scipy.optimize import least_squares
from scipy.signal import butter, sosfreqz
import orion as o
from fit_eqrbj import rbj, H
from fit_eqbands import f, z

FS = 48000
def measured(name):
    h = np.load(os.path.join(o.ROOT, 'measurements', 'scorpius', 'ir', name + '.npy'))[0].astype(np.float64)
    Hm = np.fft.rfft(h, 1 << 18); fr = np.fft.rfftfreq(1 << 18, 1 / FS)
    return np.interp(f, fr, Hm.real) + 1j * np.interp(f, fr, Hm.imag)

def model(p, kind, hpo):
    Hb = H(*rbj(kind, p[0], p[1], p[2])) - 1
    if hpo: Hb = Hb * sosfreqz(butter(hpo, p[3], 'high', fs=FS, output='sos'), worN=f, fs=FS)[1]
    return 1 + Hb

def fit(name, kind, hpo):
    M = measured(name)
    def res(p):
        d = model(p, kind, hpo) - M; return np.concatenate([d.real, d.imag])
    best = None
    for f0, q in itertools.product((60, 100, 150), (0.3, 0.6, 1.0)):
        s = least_squares(res, [f0, q, 6, 20], bounds=([10, .05, 0, 3], [2000, 10, 30, 200]))
        if best is None or s.cost < best.cost: best = s
    p = best.x; Hm = model(p, kind, hpo)
    return p, np.abs(20*np.log10(np.abs(Hm)/np.abs(M))).max(), np.abs(np.degrees(np.angle(Hm/M))).max()

if __name__ == '__main__':
    for name in ('harm_80_1.0', 'harm_50_1.0'):
        for kind, hpo in itertools.product(('peak', 'lowshelf'), (0, 2, 4)):
            p, e, pe = fit(name, kind, hpo)
            print(f'{name} {kind:8s} hp{hpo}: f0 {p[0]:8.3f} Q {p[1]:.4f} G {p[2]:.3f} hp {p[3]:.3f}  err {e:.3f} dB {pe:.2f} deg', flush=True)
