"""Drive per colour as Wiener-Hammerstein: y = Post(S(g Pre(x)) / g), both filters cascades of free biquads.
Stage 1 fits Pre x Post to the small-signal response (the low cut in Post starts from an elliptic high-pass, Pre from
flat sections); stage 2 fits every coefficient and the clip in the time domain against the reference.
    python fit_drive3.py [colour]     (writes measurements/scorpius/drive3_<colour>.json)"""
import json, os, sys
import numpy as np
from scipy.optimize import least_squares
from scipy.signal import ellip, sosfilt, sosfreqz
import orion as o
from fit_drive import signals, s, ds, OUT, FS

NPRE, NPOST = 3, 2
f = np.geomspace(10, 23000, 800); W = 2 * np.pi * f / FS; zi = np.exp(-1j * W)


def measured(colour, hp=0):
    h = np.load(os.path.join(OUT, 'ir', f'drive_{colour}_hp{hp}.npy'))[0].astype(float)
    Hf = np.fft.rfft(h, 1 << 19); fr = np.fft.rfftfreq(1 << 19, 1 / FS)
    return np.interp(f, fr, Hf.real) + 1j * np.interp(f, fr, Hf.imag)


def sos_of(v, n):
    return np.array([[v[5 * i], v[5 * i + 1], v[5 * i + 2], 1.0, v[5 * i + 3], v[5 * i + 4]] for i in range(n)])


def resp(sos):
    H = np.ones_like(zi)
    for b0, b1, b2, _, a1, a2 in sos: H *= (b0 + b1 * zi + b2 * zi ** 2) / (1 + a1 * zi + a2 * zi ** 2)
    return H


def unstable(sos):
    return max(np.abs(np.roots([1, a1, a2])).max() for _, _, _, _, a1, a2 in sos)


def model(x, pre, post, g, c, b, p):
    u = g * sosfilt(pre, x)
    sh = c * (s(u / c + b, p) - s(b, p)) / ds(b, p)
    return sosfilt(post, sh / g)


if __name__ == '__main__':
    colour = sys.argv[1] if len(sys.argv) > 1 else 'a'
    M = measured(colour)
    post0 = ellip(2 * NPOST, 1.0, 30, 42, 'high', fs=FS, output='sos')
    v_post = np.concatenate([[r[0], r[1], r[2], r[4], r[5]] for r in post0])
    v_pre = np.concatenate([[1, 0, 0, 0, 0] for _ in range(NPRE)])
    wt = 1 / np.maximum(np.abs(M), np.abs(M).max() * 0.01)
    def res1(v):
        pre = sos_of(v[:5 * NPRE], NPRE); post = sos_of(v[5 * NPRE:], NPOST)
        pen = max(0, unstable(pre) - 0.99999) + max(0, unstable(post) - 0.999999)
        d = (resp(pre) * resp(post) - M) * wt
        return np.concatenate([d.real, d.imag, [1e4 * pen]])
    best = None
    for t in range(6):
        rng = np.random.default_rng(t)
        v0 = np.concatenate([v_pre + rng.normal(0, 0.05, len(v_pre)) * (t > 0), v_post])
        s1 = least_squares(res1, v0, max_nfev=4000)
        if best is None or s1.cost < best.cost: best = s1
    v = best.x
    pre = sos_of(v[:5 * NPRE], NPRE); post = sos_of(v[5 * NPRE:], NPOST)
    Hm = resp(pre) * resp(post); m = f > 25
    print(colour, 'stage 1: response err %.3f dB %.2f deg, radii %.6f %.6f' % (
        np.abs(20 * np.log10(np.abs(Hm[m]) / np.abs(M[m]))).max(), np.abs(np.degrees(np.angle(Hm[m] / M[m]))).max(), unstable(pre), unstable(post)), flush=True)

    xs = signals()
    refs = o.render_many([dict(x=x, params=o.ONLY('drive', hipass=0), hidden={'drive.driveMode': colour}) for x in xs], cache=True)
    xs = [x.astype(float) for x in xs]
    norm = [np.sqrt(np.mean(r[0, o.SKIP:] ** 2)) for r in refs]
    def res2(u):
        pre = sos_of(u[4:4 + 5 * NPRE], NPRE); post = sos_of(u[4 + 5 * NPRE:], NPOST)
        if unstable(pre) > 0.99999 or unstable(post) > 0.999999: return np.full(sum(len(x[o.SKIP::4]) for x in xs), 10.0)
        r = []
        for x, ref, nm in zip(xs, refs, norm):
            d_ = (model(x, pre, post, *u[:4]) - ref[0])[o.SKIP::4]
            r.append(d_ / nm / np.sqrt(len(d_)))
        return np.concatenate(r)
    u0 = np.concatenate([[1.5, 1.0, 0.1, 2.0], v])
    s2 = least_squares(res2, u0, x_scale='jac', max_nfev=60)
    u = s2.x
    pre = sos_of(u[4:4 + 5 * NPRE], NPRE); post = sos_of(u[4 + 5 * NPRE:], NPOST)
    nulls = [o.null_db(ref[:1], model(x, pre, post, *u[:4])[None]) for x, ref in zip(xs, refs)]
    print(colour, 'stage 2: clip', np.round(u[:4], 4).tolist(), 'nulls', ' '.join('%.1f' % n for n in nulls), flush=True)
    json.dump(dict(clip=u[:4].tolist(), pre=pre.tolist(), post=post.tolist(), nulls=nulls), open(os.path.join(OUT, f'drive3_{colour}.json'), 'w'))
