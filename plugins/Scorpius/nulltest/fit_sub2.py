"""Sub parallel: its path as gain x Butterworth low-pass (order n) x Butterworth high-pass (order m), designed with
scipy (bilinear, prewarped), fitted in magnitude and phase to the high-SNR response (par_sub_hi.npy)."""
import itertools, os
import numpy as np
from scipy.optimize import least_squares
from scipy.signal import butter, sosfreqz
import orion as o

h = np.load(os.path.join(o.ROOT, 'measurements', 'scorpius', 'ir', 'par_sub_hi.npy'))
FS = 48000
f = np.geomspace(5, 2000, 500)
H = np.fft.rfft(h, 1 << 19); fr = np.fft.rfftfreq(1 << 19, 1 / FS)
T = np.interp(f, fr, H.real) + 1j * np.interp(f, fr, H.imag)
m = np.abs(T) > np.abs(T).max() * 10 ** (-40 / 20)

def model(p, n, k, kinds=('butter', 'butter')):
    g, fl, fh = p[:3]
    sos = np.concatenate([butter(n, fl, 'low', fs=FS, output='sos'), butter(k, fh, 'high', fs=FS, output='sos')])
    return g * sosfreqz(sos, worN=f, fs=FS)[1]

def fit(n, k):
    def res(p):
        d = (model(p, n, k) - T)[m] / np.abs(T[m]); return np.concatenate([d.real, d.imag])
    s = least_squares(res, [2, 150, 14], bounds=([0, 50, 3], [5, 1000, 60]))
    Hm = model(s.x, n, k)
    return s.x, np.abs(20 * np.log10(np.abs(Hm[m]) / np.abs(T[m]))).max(), np.abs(np.degrees(np.angle(Hm[m] / T[m]))).max()

if __name__ == '__main__':
    for n, k in itertools.product((4, 5, 6, 7, 8), (4, 6, 8)):
        p, e, pe = fit(n, k)
        print(f'LP {n} HP {k}: gain {p[0]:.4f} fl {p[1]:.2f} fh {p[2]:.3f}  err {e:.2f} dB {pe:.1f} deg', flush=True)
