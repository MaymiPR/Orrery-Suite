"""The transition curve of a parameter jump (autom_probe.py's cases): the output during the jump as a blend
(1 - a) y0 + a y1 of the steady outputs at the two values, a fitted in 16-sample windows; for the reference and for
ours. Prints where a crosses 10 / 50 / 90 % relative to the jump (samples) and writes the curves to
measurements/scorpius/autom/<case>_<block>.npz.
    python autom_curve.py case [block]"""
import os, sys
import numpy as np
import orion as o
from autom_probe import cases, PRE, T1, T2
from order import drums

W = 16


def curve(y, y0, y1, t, span=1200):
    a = []
    for s in range(PRE + t - 200, PRE + t + span, W):
        d = (y1 - y0)[:, s:s + W].ravel(); e = (y - y0)[:, s:s + W].ravel()
        a.append(np.dot(d, e) / (np.dot(d, d) + 1e-20))
    return np.array(a)


def cross(a, q):
    i = np.argmax(a >= q) if np.any(a >= q) else -1
    return -200 + i * W if i >= 0 else None


if __name__ == '__main__':
    name = sys.argv[1]; block = int(sys.argv[2]) if len(sys.argv) > 2 else 512
    x = drums()[:PRE + 3 * o.FS] * 10 ** (-12 / 20)
    params, p, v0, v1 = cases()[name]
    job = dict(x=x, params=params, auto=[(p, PRE + T1, v1), (p, PRE + T2, v0)])
    out = {}
    for who in ('ref', 'model'):
        y = o.render_many([job], who, block=block, cache=True)[0]
        y0 = o.render_many([dict(x=x, params=dict(params, **{p: v0}))], who, block=block, cache=True)[0]
        y1 = o.render_many([dict(x=x, params=dict(params, **{p: v1}))], who, block=block, cache=True)[0]
        up = curve(y, y0, y1, T1); dn = 1 - curve(y, y0, y1, T2)
        out[who] = (up, dn)
        print(f'{name} block {block} {who:5s} up: 10/50/90 % at', [cross(up, q) for q in (0.1, 0.5, 0.9)],
              ' down:', [cross(dn, q) for q in (0.1, 0.5, 0.9)])
    d = os.path.join(o.ROOT, 'measurements', 'scorpius', 'autom'); os.makedirs(d, exist_ok=True)
    np.savez(os.path.join(d, f'{name.replace(" ", "_")}_{block}.npz'), **{f'{k}_{i}': v for k, (a, b) in out.items() for i, v in (('up', a), ('down', b))})
