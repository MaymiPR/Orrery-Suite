"""Identifies the reference's EQ bands: fits shelf / bell filters to the measured complex responses (EQ stage only,
re the flat path). Two candidate forms: RBJ cookbook biquads (bilinear) and the analog prototype through the
bilinear transform with the corner pre-warped (what a Cytomic / SVF shelf does), each with free f0 and Q."""
import json, os
import numpy as np
from scipy.optimize import least_squares
from scipy.signal import freqz
import tgp

FS = tgp.FS
PRE = 2 * FS; N = 65536
f = np.fft.rfftfreq(N, 1 / FS)
sel = (f > 10) & (f < 23000)


def measure(params):
    x = np.zeros(PRE + N, np.float32); x[PRE] = 0.01
    return x, dict(x=x, norm=tgp.EQ_ONLY, params=params, raw=True)


jobs = [measure({})[1]] + [measure({b: g})[1] for b in ('EQ Low Gain', 'EQ Mid Gain', 'EQ High Gain') for g in (-6, 6)]
ys = tgp.render_many(jobs)
H0 = np.fft.rfft(ys[0][0, PRE:PRE + N])
meas = {}
i = 1
for b in ('low', 'mid', 'high'):
    for g in (-6, 6):
        meas[(b, g)] = np.fft.rfft(ys[i][0, PRE:PRE + N]) / H0; i += 1


def rbj(kind, f0, q, gain_db, fs=FS):
    A = 10 ** (gain_db / 40); w = 2 * np.pi * f0 / fs; c, s = np.cos(w), np.sin(w); al = s / (2 * q)
    if kind == 'peak':
        b = [1 + al * A, -2 * c, 1 - al * A]; a = [1 + al / A, -2 * c, 1 - al / A]
    elif kind == 'low':
        sq = 2 * np.sqrt(A) * al
        b = [A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq)]
        a = [(A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq]
    else:
        sq = 2 * np.sqrt(A) * al
        b = [A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq)]
        a = [(A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq]
    return np.array(b) / a[0], np.array(a) / a[0]


def svf(kind, f0, q, gain_db, fs=FS):
    """Cytomic (Andrew Simper) SVF shelf / bell, as digital biquad coefficients."""
    A = 10 ** (gain_db / 40); g = np.tan(np.pi * f0 / fs)
    if kind == 'peak':
        k = 1 / (q * A); m0, m1, m2 = 1, k * (A * A - 1), 0
    elif kind == 'low':
        g /= np.sqrt(A); k = 1 / q; m0, m1, m2 = 1, k * (A - 1), A * A - 1
    else:
        g *= np.sqrt(A); k = 1 / q; m0, m1, m2 = A * A, k * (1 - A) * A, 1 - A * A
    # transfer function of the trapezoidal SVF: H(z) from the analog prototype with g = tan(pi f0 / fs)
    # analog: H(s) = m0 + m1 * (s k_hp...) -> use bilinear of H(s) = (m0 s^2 + (m0 k + m1) s + (m0 + m2)) / (s^2 + k s + 1), s normalised
    num = np.array([m0, m0 * k + m1, m0 + m2]); den = np.array([1.0, k, 1.0])
    # bilinear with s = (1/g) (1 - z^-1) / (1 + z^-1)
    def bil(p):
        c2, c1, c0 = p[0] / g ** 2, p[1] / g, p[2]
        return np.array([c2 + c1 + c0, 2 * (c0 - c2), c2 - c1 + c0])
    b, a = bil(num), bil(den)
    return b / a[0], a / a[0]


out = {}
for kind in ('low', 'mid', 'high'):
    k = 'peak' if kind == 'mid' else kind
    for name, form in (('rbj', rbj), ('svf', svf)):
        def resid(p):
            r = []
            for g in (-6, 6):
                b, a = form(k, p[0], p[1], g)
                _, h = freqz(b, a, worN=f[sel], fs=FS)
                Hm = meas[(kind, g)][sel]
                r.append(20 * np.log10(np.abs(h) / np.abs(Hm)))
                r.append(np.angle(h / Hm) * 20)
            return np.concatenate(r)
        x0 = {'low': [180, 0.7], 'mid': [640, 0.5], 'high': [10000, 0.7]}[kind]
        r = least_squares(resid, x0, bounds=([5, 0.05], [23000, 20]))
        mag = np.concatenate([r.fun[0:sel.sum()], r.fun[2 * sel.sum():3 * sel.sum()]])
        out[f'{kind}/{name}'] = dict(f0=round(float(r.x[0]), 3), q=round(float(r.x[1]), 5),
                                     max_mag_err_dB=round(float(np.max(np.abs(mag))), 4), rms=round(float(np.sqrt(np.mean(r.fun ** 2))), 5))
        print(kind, name, out[f'{kind}/{name}'])
json.dump(out, open(os.path.join(tgp.ROOT, 'measurements', 'eq', 'eq_fit.json'), 'w'), indent=1)
