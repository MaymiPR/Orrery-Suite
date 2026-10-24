"""Per-frame LED states of the I/O meters in a RefCapture film at 100 %: 40 LEDs per channel (8 x 1 px on a 4 px pitch,
bottom LED at y 248; columns x 25 / 61 input, 821 / 857 output). Returns (frames, channels, 40) codes:
0 off, 1 green (#00824B), 2 mint (#8EDBC4)."""
import numpy as np
from filmtools import Film

COLS = {'in': (25, 61), 'out': (821, 857)}


def classify(px):
    r, g, b = px[..., 0].astype(int), px[..., 1].astype(int), px[..., 2].astype(int)
    code = np.zeros(px.shape[:-1], int)
    code[(g > 90) & (r < 60)] = 1
    code[(g > 150) & (r > 100)] = 2
    return code


def leds(film, which='in'):
    f = Film(film) if isinstance(film, str) else film
    ys = 248 - 4 * np.arange(40)
    out = np.zeros((f.n, 2, 40), int)
    for i in range(f.n):
        fr = f.frame(i)
        for c, x in enumerate(COLS[which]):
            out[i, c] = classify(fr[ys, x + 4])
    return f.t, out
