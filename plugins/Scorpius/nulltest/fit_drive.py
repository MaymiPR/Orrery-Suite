"""Drive per colour, simulated in Python and fitted to the reference in the time domain:
    y = L(S(g x) / g)
L the small-signal response (vector-fitted to linear.py's impulse response, parallel sections), S an asymmetric soft
clip with unit slope at 0: S(u) = c (s(u / c + b) - s(b)) / s'(b), s(v) = v / (1 + |v|^p)^(1/p).
    python fit_drive.py [colour ...]      (writes measurements/scorpius/drive_<colour>.json)"""
import json, os, sys
import numpy as np
from scipy.optimize import least_squares
from scipy.signal import lfilter
import orion as o
import nullmod as N
from vecfit import fit_sections, parallel_response

FS = 48000
OUT = os.path.join(o.ROOT, 'measurements', 'scorpius')


def linear(colour, hp=0, order=14):
    h = np.load(os.path.join(OUT, 'ir', f'drive_{colour}_hp{hp}.npy'))[0].astype(float)
    f = np.geomspace(8, 23000, 1500); w = 2 * np.pi * f / FS
    Hf = np.fft.rfft(h, 1 << 19); fr = np.fft.rfftfreq(1 << 19, 1 / FS)
    H = np.interp(f, fr, Hf.real) + 1j * np.interp(f, fr, Hf.imag)
    return fit_sections(w, H, order, 1 / np.maximum(np.abs(H), np.abs(H).max() * 0.01))


def apply_linear(d, secs, x):
    y = d * x
    for b0, b1, a1, a2 in secs: y = y + lfilter([b0, b1], [1, a1, a2], x)
    return y


def s(v, p): return v / (1 + np.abs(v) ** p) ** (1 / p)
def ds(v, p): return (1 + np.abs(v) ** p) ** (-1 - 1 / p)


def model(x, lin, g, c, b, p):
    u = g * x
    sh = c * (s(u / c + b, p) - s(b, p)) / ds(b, p)
    return apply_linear(*lin, sh / g)


def signals():
    sig = N.signals()
    xs = [v if v.ndim == 1 else v[0] for v in sig.values()]
    for f_ in (100, 1000, 5000):
        for lv in (-6, 0, 6):
            xs.append(o.tone(f_, 0.5, lv))
    return xs


if __name__ == '__main__':
    xs = signals()
    for colour in sys.argv[1:] or 'abc':
        refs = o.render_many([dict(x=x, params=o.ONLY('drive', hipass=0), hidden={'drive.driveMode': colour}) for x in xs], cache=True)
        lin = linear(colour)
        def res(v):
            r = []
            for x, ref in zip(xs, refs):
                y = model(x.astype(float), lin, *v)
                d = (y - ref[0])[o.SKIP::3]
                r.append(d / np.sqrt(np.mean(ref[0, o.SKIP:] ** 2)) / np.sqrt(len(d)))
            return np.concatenate(r)
        s_ = least_squares(res, [1.5, 1.0, 0.1, 2.0], bounds=([0.05, 0.05, -2, 0.5], [20, 10, 2, 10]), x_scale='jac')
        nulls = [o.null_db(ref[:1], model(x.astype(float), lin, *s_.x)[None]) for x, ref in zip(xs, refs)]
        print(colour, 'g %.4f c %.4f b %.4f p %.4f' % tuple(s_.x), 'nulls', ' '.join('%.1f' % v for v in nulls), flush=True)
        json.dump(dict(params=s_.x.tolist(), d=lin[0], secs=[list(map(float, t)) for t in lin[1]], nulls=nulls),
                  open(os.path.join(OUT, f'drive_{colour}.json'), 'w'))
