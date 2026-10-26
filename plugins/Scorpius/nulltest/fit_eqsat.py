"""Harmonic EQ, saturation: y = x + c S(k BP(x) / c), the band (its fitted band-pass, its gain k from the knob) through
an odd shaper S(u) = u / (1 + |u|^p)^(1/p) with ceiling c. Fitted to the fundamental's gain and the 3rd and 5th
harmonics of tones (eq_tones.json: 1 kHz at +2 / +4 / +6, 300 Hz and 3 kHz at +6, -40 ... +4 dBFS).
    python fit_eqsat.py"""
import json, os
import numpy as np
from scipy.optimize import least_squares
from scipy.signal import lfilter
import orion as o
from fit_eq import biquad

D = json.load(open(os.path.join(o.ROOT, 'measurements', 'scorpius', 'eq_tones.json')))
G = {0.5: 0.634, 1: 1.258, 2: 2.482, 3: 3.677, 4: 4.846, 5: 5.994, 6: 7.124}     # knob dB -> centre gain dB (measured)
FC, Q = 997.56, 1.001
b, a = biquad('bp', FC, Q)


def shaper(u, p):
    return u / (1 + np.abs(u) ** p) ** (1 / p)


def simulate(ft, lv, g, c, p, post=None):
    x = o.tone(ft, 0.5, lv).astype(np.float64)
    k = 10 ** (G[g] / 20) - 1
    band = k * lfilter(b, a, x)
    y = x + c * shaper(band / c, p)
    if post is not None: y = x + lfilter(*post, y - x)
    h = o.harmonics(y[12000:], ft, count=5)
    return h[0] - lv, h[2] - h[0], h[4] - h[0]


def residual(par):
    c, p = par
    r = []
    for key, rows in D.items():
        g, ft = key.split('_'); g = float(g); ft = int(ft)
        g = int(g) if g == int(g) else g
        for lv, gain, h3, h5 in rows:
            if lv < -30: continue
            mg, m3, m5 = simulate(ft, lv, g, c, p)
            r += [mg - gain, 0.05 * (m3 - h3) if h3 > -90 else 0.0]
    return np.array(r)


if __name__ == '__main__':
    s = least_squares(residual, [0.45, 2.0], bounds=([0.05, 0.5], [5, 8]))
    c, p = s.x
    print(f'ceiling c {c:.4f}, knee p {p:.3f}, rms residual {np.sqrt(np.mean(s.fun ** 2)):.3f}')
    for key, rows in D.items():
        g, ft = key.split('_'); g = int(float(g)); ft = int(ft)
        print(key, ' '.join(f'{lv}:{gain:+.2f}/{simulate(ft, lv, g, c, p)[0]:+.2f} h3 {h3:.0f}/{simulate(ft, lv, g, c, p)[1]:.0f}'
                            for lv, gain, h3, h5 in rows[5::4]))
