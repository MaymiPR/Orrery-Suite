"""Lift at amount 1 (the wet path), simulated in Python and fitted to the reference in the time domain. Forms:
    hammer: y = L(S(g x) / g)      wiener: y = S(g L(x)) / g
L the small-signal response (vector fit of linear.py's lift_1.0), S an asymmetric soft clip that keeps a share l of
its input: S(u) = (1 - l) c (s(u / c + b) - s(b)) / s'(b) + l u.
    python fit_lift.py"""
import os, sys
import numpy as np
from scipy.optimize import least_squares
from scipy.signal import lfilter
import orion as o
from fit_drive import s, ds, OUT, FS, signals
from vecfit import fit_sections


def linear(name, order=10):
    h = np.load(os.path.join(OUT, 'ir', name + '.npy'))[0].astype(float)
    f = np.geomspace(8, 23000, 1500); w = 2 * np.pi * f / FS
    Hf = np.fft.rfft(h, 1 << 19); fr = np.fft.rfftfreq(1 << 19, 1 / FS)
    H = np.interp(f, fr, Hf.real) + 1j * np.interp(f, fr, Hf.imag)
    return fit_sections(w, H, order, 1 / np.maximum(np.abs(H), np.abs(H).max() * 0.01))


def apply(d, secs, x):
    y = d * x
    for b0, b1, a1, a2 in secs: y = y + lfilter([b0, b1], [1, a1, a2], x)
    return y


def clip(u, c, b, p, l):
    return (1 - l) * c * (s(u / c + b, p) - s(b, p)) / ds(b, p) + l * u


def model(form, x, lin, Lx, g, c, b, p, l):
    if form == 'hammer': return apply(*lin, clip(g * x, c, b, p, l) / g)
    return clip(g * Lx, c, b, p, l) / g


if __name__ == '__main__':
    xs = [x.astype(float) for x in signals()]
    refs = o.render_many([dict(x=x.astype(np.float32), params={**o.ONLY('lift'), 'Lift Amount': 1}) for x in xs], cache=True)
    lin = linear('lift_1.0')
    Lxs = [apply(*lin, x) for x in xs]
    for form in ('hammer', 'wiener'):
        def res(v):
            r = []
            for x, Lx, ref in zip(xs, Lxs, refs):
                d_ = (model(form, x, lin, Lx, *v) - ref[0])[o.SKIP::3]
                r.append(d_ / np.sqrt(np.mean(ref[0, o.SKIP:] ** 2)) / np.sqrt(len(d_)))
            return np.concatenate(r)
        st = least_squares(res, [1.0, 1.0, 0.05, 2.0, 0.05], bounds=([0.05, 0.05, -2, 0.5, 0], [20, 10, 2, 10, 0.9]), x_scale='jac')
        n = [o.null_db(ref[:1], model(form, x, lin, Lx, *st.x)[None]) for x, Lx, ref in zip(xs, Lxs, refs)]
        print(form, np.round(st.x, 4).tolist(), 'nulls', ' '.join('%.1f' % v for v in n), flush=True)
