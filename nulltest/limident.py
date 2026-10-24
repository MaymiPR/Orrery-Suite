"""The limiter's inter-sample interpolator identified without a model of its shape: isolated random band-limited pulses
(peak about 2, above the ceiling); each one's envelope maximum is the level at its loudest point (limpulse.py), i.e.
|sum_i h_phase[i] x[n + i - c]| for the phase and sample that give the largest value. Starting from the plug-in's
table, the phase and sample of each pulse's maximum are found, each phase's taps are solved by least squares on its
pulses, and the two steps repeat.

    python limident.py [taps] [phases]"""
import sys
import numpy as np
import tgp, limpy, limpulse as P

FS = tgp.FS; PRE = FS; GAP = 1200; NPULSE = 4096
TRIM = P.TRIM


def pulses(seed=11):
    rng = np.random.default_rng(seed)
    n = PRE + GAP * NPULSE; x = np.zeros(n); at = []
    for i in range(NPULSE):
        L = 33
        if rng.uniform() < 0.3:          # a short raw noise burst: content up to Nyquist
            h2 = np.zeros(L); m = rng.integers(2, 7); h2[16:16 + m] = rng.standard_normal(m)
        else:
            fc = rng.uniform(0.12, 0.5)
            k = np.arange(L) - L // 2 - rng.uniform(0, 1)
            h = 2 * fc * np.sinc(2 * fc * k) * np.kaiser(L, 6)
            h2 = np.convolve(h, rng.standard_normal(3) * [0.3, 1, 0.3])[:L] if rng.uniform() < 0.5 else h
        p = h2 / np.max(np.abs(h2)) * rng.uniform(1.6, 2.6) * rng.choice([-1, 1])
        a = PRE + i * GAP; x[a:a + L] = p; at.append(a)
    return x, at


def windows(xin, at, taps, span=40):
    """for each pulse: the candidate samples n near its peak and the input windows x[n - c .. n - c + taps)"""
    c = taps // 2 - 1
    out = []
    for a in at:
        lo = a + np.argmax(np.abs(xin[a:a + 300])) - 3
        ns = np.arange(lo, lo + 7)
        out.append(np.stack([xin[q - c:q - c + taps] for q in ns]))
    return out


def fit(taps=16, sub=16, iters=24):
    x, at = pulses()
    xin = P.limiter_input(x)
    y = tgp.render_many([dict(x=np.stack([x, x]).astype(np.float32), norm=tgp.LIM_ONLY, params={'Limiter Input Gain': 0})], 'ref', cache=True)[0][0]
    ref = P.read(xin, y, at)
    # only pulses whose reading is the level for our own plug-in (no plan or slow envelope in the way)
    ym = tgp.render_many([dict(x=np.stack([x, x]).astype(np.float32), norm=tgp.LIM_ONLY, params={'Limiter Input Gain': 0})], 'model')[0][0]
    ok = np.abs(P.read(xin, ym, at) - P.port_levels(xin, at)) < 1e-6
    print(f'{ok.sum()} of {len(at)} pulses usable', flush=True)
    at = [a for a, o in zip(at, ok) if o]; ref = ref[ok]
    W = windows(xin, at, taps)
    # start: a Kaiser sinc of this size
    T = np.zeros((sub, taps))
    for ph in range(sub):
        k = np.arange(taps) - (taps // 2 - 1) - ph / sub; r = k / (taps / 2)
        w = np.where(np.abs(r) >= 1, 0, np.i0(6 * np.sqrt(np.clip(1 - r * r, 0, None))) / np.i0(6))
        T[ph] = np.sinc(k) * w; T[ph] /= T[ph].sum()
    for it in range(iters):
        rows = [[] for _ in range(sub)]; rhs = [[] for _ in range(sub)]; pred = []
        for wv, r in zip(W, ref):
            v = wv @ T.T                                       # (samples, phases)
            j, ph = np.unravel_index(np.argmax(np.abs(v)), v.shape)
            pred.append(max(1.0, abs(v[j, ph])))
            top = np.sort(np.abs(v).ravel())[-2:]
            if top[1] - top[0] > 1e-3 and abs(abs(v[j, ph]) - r) < 0.05:     # an unambiguous maximum, no outlier
                rows[ph].append(np.sign(v[j, ph]) * wv[j]); rhs[ph].append(r)
        err = np.array(pred) - ref
        print(f'iter {it}: rms {np.sqrt(np.mean(err ** 2)):.3e} max {np.max(np.abs(err)):.3e}  pulses per phase {[len(q) for q in rows]}', flush=True)
        for ph in range(sub):
            if len(rows[ph]) >= taps:
                A = np.array(rows[ph]); b = np.array(rhs[ph])
                # ridge toward the current taps keeps thinly covered phases sane
                lam = 1e-6
                T[ph] = 0.5 * T[ph] + 0.5 * np.linalg.solve(A.T @ A + lam * np.eye(taps), A.T @ b + lam * T[ph])   # damped
    return T, err


if __name__ == '__main__':
    taps = int(sys.argv[1]) if len(sys.argv) > 1 else 16
    sub = int(sys.argv[2]) if len(sys.argv) > 2 else 16
    T, err = fit(taps, sub)
    np.save(f'{tgp.ROOT}/measurements/limiter_interp_{taps}x{sub}.npy', T)
    np.set_printoptions(precision=5, suppress=True, linewidth=200)
    print('phase sums', T.sum(1))
    print('phase 0', T[0])
