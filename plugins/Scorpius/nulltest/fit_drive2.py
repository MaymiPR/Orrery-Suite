"""Drive per colour as Wiener-Hammerstein: y = Post(S(g Pre(x)) / g). The small-signal response L (vector fit) is split
into poles and zeros: those below f_split (the low cut) go after the clip, the rest before it. The clip S as in
fit_drive.py. Fitted in the time domain against the reference for several splits.
    python fit_drive2.py [colour]"""
import json, os, sys
import numpy as np
from scipy.optimize import least_squares
from scipy.signal import zpk2sos, sosfilt, sosfreqz
import orion as o
from fit_drive import signals, s, ds, OUT, FS
from vecfit import vecfit


def zpk_of(colour, order=14, hp=0):
    h = np.load(os.path.join(OUT, 'ir', f'drive_{colour}_hp{hp}.npy'))[0].astype(float)
    f = np.geomspace(8, 23000, 1500); w = 2 * np.pi * f / FS
    Hf = np.fft.rfft(h, 1 << 19); fr = np.fft.rfftfreq(1 << 19, 1 / FS)
    H = np.interp(f, fr, Hf.real) + 1j * np.interp(f, fr, Hf.imag)
    d, poles, res = vecfit(w, H, order, 40, 1 / np.maximum(np.abs(H), np.abs(H).max() * 0.01))
    # numerator of d + sum r / (1 - p z^-1) in z^-1, then its roots (zeros in z)
    den = np.poly(poles)                                    # prod (z - p) as coefficients in z^-1 order
    num = d * den.astype(complex)
    for k, (p, r) in enumerate(zip(poles, res)):
        num = num + r * np.concatenate([np.poly(np.delete(poles, k)), [0]])
    zeros = np.roots(num)
    gain = num[0].real
    return zeros, poles, gain, (w, H)


def conj_clean(v, tol=1e-7):
    """exact conjugate pairs: the upper half-plane values and their conjugates, the near-real ones real"""
    up = v[v.imag > tol]
    re = v[np.abs(v.imag) <= tol].real + 0j
    return np.concatenate([up, np.conj(up), re])


def split(zeros, poles, gain, fsplit):
    zeros = conj_clean(zeros); poles = conj_clean(poles)
    """the low cut: poles and as many zeros below fsplit (by angle); the rest: the pre-filter"""
    wcut = 2 * np.pi * fsplit / FS
    lowp = np.abs(np.angle(poles)) < wcut
    # zeros nearest 0 Hz, whole conjugate pairs, as many as the low poles
    zeros = np.where(np.abs(zeros.imag) < 1e-9, zeros.real + 0j, zeros)
    lowz = np.zeros(len(zeros), bool)
    for i in np.argsort(np.abs(np.angle(zeros)) + 10 * np.abs(np.abs(zeros) - 1)):
        if lowz.sum() >= lowp.sum(): break
        if lowz[i]: continue
        lowz[i] = True
        if zeros[i].imag != 0:
            j = int(np.argmin(np.abs(zeros - np.conj(zeros[i])) + lowz * 1e9)); lowz[j] = True
    poles = np.where(np.abs(poles.imag) < 1e-9, poles.real + 0j, poles)
    post = zpk2sos(zeros[lowz], poles[lowp], 1.0)
    pre = zpk2sos(zeros[~lowz], poles[~lowp], 1.0)
    # gains: post unity at 1 kHz, the rest in pre
    w1 = 2 * np.pi * 1000 / FS
    gpost = 1 / abs(sosfreqz(post, worN=[w1])[1][0])
    post[0, :3] *= gpost
    return pre, post


def model(x, pre, post, k, g, c, b, p):
    u = g * sosfilt(pre, x) * k
    sh = c * (s(u / c + b, p) - s(b, p)) / ds(b, p)
    return sosfilt(post, sh / g)


if __name__ == '__main__':
    colour = sys.argv[1] if len(sys.argv) > 1 else 'a'
    xs = signals()
    refs = o.render_many([dict(x=x, params=o.ONLY('drive', hipass=0), hidden={'drive.driveMode': colour}) for x in xs], cache=True)
    zeros, poles, gain, (w, H) = zpk_of(colour)
    for fsplit in (30, 60, 100, 200, 400):
        pre, post = split(zeros, poles, gain, fsplit)
        # the overall gain k so that pre x post matches L at 1 kHz
        w1 = 2 * np.pi * 1000 / FS
        Lat = np.interp(w1, w, H.real) + 1j * np.interp(w1, w, H.imag)
        k = abs(Lat) / abs(sosfreqz(pre, worN=[w1])[1][0] * sosfreqz(post, worN=[w1])[1][0])
        def res(v):
            r = []
            for x, ref in zip(xs, refs):
                y = model(x.astype(float), pre, post, k, *v)
                d_ = (y - ref[0])[o.SKIP::3]
                r.append(d_ / np.sqrt(np.mean(ref[0, o.SKIP:] ** 2)) / np.sqrt(len(d_)))
            return np.concatenate(r)
        try:
            s_ = least_squares(res, [1.5, 1.0, 0.1, 2.0], bounds=([0.05, 0.05, -2, 0.5], [20, 10, 2, 10]), x_scale='jac', max_nfev=200)
        except Exception as e:
            print(fsplit, 'failed', e); continue
        nulls = [o.null_db(ref[:1], model(x.astype(float), pre, post, k, *s_.x)[None]) for x, ref in zip(xs, refs)]
        print(colour, 'split', fsplit, np.round(s_.x, 4).tolist(), 'nulls', ' '.join('%.1f' % v for v in nulls), flush=True)
