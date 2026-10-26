"""Drive per colour: y = Post(S(g Pre(x)) / g) with Post the low cut alone (an elliptic high-pass, order 4: ripple,
stopband, corner) and Pre the rest of the small-signal response, Pre = L / Post (vector-fitted each time); the clip S
as in fit_drive.py. The waveforms show it: a 1 kHz tone at +6 dBFS comes out with flat tops at +0.49 / -0.43 (the
clip after the shaping and its phase lead), a 100 Hz one with drooping tops (a high-pass after the clip).
    python fit_drive4.py [colour]     (writes measurements/scorpius/drive4_<colour>.json)"""
import json, os, sys
import numpy as np
from scipy.optimize import minimize
from scipy.signal import ellip, sosfilt, sosfreqz, lfilter
import orion as o
from fit_drive import signals, s, ds, OUT, FS
from vecfit import fit_sections, parallel_response

f = np.geomspace(8, 23000, 1200); W = 2 * np.pi * f / FS
MINF, POSTORDER = 12.0, 2


def measured(colour, hp=0):
    h = np.load(os.path.join(OUT, 'ir', f'drive_{colour}_hp{hp}.npy'))[0].astype(float)
    Hf = np.fft.rfft(h, 1 << 19); fr = np.fft.rfftfreq(1 << 19, 1 / FS)
    return np.interp(f, fr, Hf.real) + 1j * np.interp(f, fr, Hf.imag)


def parts(L, rp, rs, fc, order=None, preorder=14):
    order = order or POSTORDER
    post = ellip(order, rp, rs, fc, 'high', fs=FS, output='sos')
    P = sosfreqz(post, worN=W)[1]
    target = L / P
    m = f > MINF                             # Pre over the whole band: it carries part of the low cut
    d, secs = fit_sections(W[m], target[m], preorder, 1 / np.maximum(np.abs(target[m]), 0.05))
    return post, (d, secs)


def apply_parallel(d, secs, x):
    y = d * x
    for b0, b1, a1, a2 in secs: y = y + lfilter([b0, b1], [1, a1, a2], x)
    return y


def model(x, post, pre, g, c, b, p):
    u = g * apply_parallel(*pre, x)
    sh = c * (s(u / c + b, p) - s(b, p)) / ds(b, p)
    return sosfilt(post, sh / g)


if __name__ == '__main__':
    colour = sys.argv[1] if len(sys.argv) > 1 else 'a'
    L = measured(colour)
    xs = [x.astype(float) for x in signals()]
    refs = o.render_many([dict(x=x, params=o.ONLY('drive', hipass=0), hidden={'drive.driveMode': colour}) for x in xs], cache=True)
    def nulls(v):
        rp, rs, fc, g, c, b, p = v
        if rp <= 0.01 or rs <= 5 or fc <= 10 or c <= 0.01 or p <= 0.5 or g <= 0.01: return [0.0] * len(xs)
        try: post, pre = parts(L, rp, rs, fc)
        except Exception: return [0.0] * len(xs)
        return [o.null_db(ref[:1], model(x, post, pre, g, c, b, p)[None]) for x, ref in zip(xs, refs)]
    best = [None, 1e9]
    def cost(v):
        n = nulls(v); c_ = float(np.mean(n))
        if c_ < best[1]: best[0], best[1] = list(v), c_; print('%.2f' % c_, np.round(v, 4).tolist(), flush=True)
        return c_
    v0 = [1.0, 30.0, 40.0, 2.0, 0.5, -0.05, 8.0]
    sres = minimize(cost, v0, method='Nelder-Mead', options=dict(maxiter=500, xatol=1e-3, fatol=0.02))
    n = nulls(best[0])
    print(colour, 'best', np.round(best[0], 4).tolist(), 'nulls', ' '.join('%.1f' % v for v in n))
    post, pre = parts(L, *best[0][:3])
    json.dump(dict(v=best[0], post=post.tolist(), pre_d=pre[0], pre=[list(map(float, t)) for t in pre[1]], nulls=n),
              open(os.path.join(OUT, f'drive4_{colour}.json'), 'w'))
