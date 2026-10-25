"""The high band's shaper drive k and compressor gain n read apart, model-free: a steady 5.5 kHz pump (inside the
band) with a quiet 16 kHz probe (inverted-probe pairs, hightrace.py). With u = n s and the shaper sin(k u) / k,
  probe gain re probe alone      P = n J0(z)              (the shaper's slope cos(k u), averaged)
  pump's 3rd harmonic / make-up   H3 = 2 J3(z) / k        (16.5 kHz)
with z = k n A (A: the pump's amplitude in the band). Two equations per level give n and k. The make-up M is the probe's
gain alone. Run for the reference and ours (whose k is known: a check on the method).

    python shaperid.py [amount 0..1] [only=model] [tuning key=value ...] (tuning for ours)"""
import sys
import numpy as np
from scipy.special import jv
from scipy.optimize import brentq
import tgp, ident as I
from hightrace import FP, PROBE

FS = tgp.FS; PRE = FS
FPUMP = 5500.0
LEVELS = [-18, -15, -12, -9, -6, -3, 0]


def band_gain(f):
    H = np.fft.rfft(I.IRS[2], FS)
    return abs(H[int(f)])


def tone(y, f):
    seg = y[-FS:]                                  # last second: integer-Hz tones sit on bins
    w = np.hanning(FS)
    return 2 * np.sum(seg * w * np.exp(-2j * np.pi * f * np.arange(FS) / FS)) / np.sum(w)


def high_share(f, node=8):
    """the high band's share of the section's small-signal response at f (the other bands pass the probe too: the
    mid band's linear part is -28 dB at 16 kHz, uncompressed)"""
    H = [np.fft.rfft(h, FS)[int(f)] for h in I.IRS]
    M = [10 ** (I.GAINS16[b][node] / 20) for b in range(3)]
    return M[2] * H[2] / sum(m * h for m, h in zip(M, H))


def solve(P, H3, A):
    """n and k from P = n J0(k n A) and H3 = 2 J3(k n A) / k (on a grid of n; k from H3 for each n)"""
    best = (np.inf, np.nan, np.nan)
    for n in np.linspace(0.2, 1.05, 8501):
        f = lambda k: 2 * jv(3, k * n * A) / k - H3
        ks = np.linspace(0.05, min(6.0, 3.5 / (n * A)), 200); v = f(ks)
        j = np.nonzero(np.sign(v[:-1]) != np.sign(v[1:]))[0]
        if len(j) == 0: continue
        k = brentq(f, ks[j[0]], ks[j[0] + 1])
        e = abs(n * jv(0, k * n * A) - P)
        if e < best[0]: best = (e, n, k)
    return best[1], best[2]


if __name__ == '__main__':
    amt = float(sys.argv[1]) if len(sys.argv) > 1 and '=' not in sys.argv[1] else 0.5
    opts = dict(a.split('=', 1) for a in sys.argv[1:] if '=' in a)
    only = opts.pop('only', None)
    tune = {k: float(v) for k, v in opts.items()}
    n_s = PRE + 2 * FS; t = np.arange(n_s) / FS
    probe = PROBE * np.sin(2 * np.pi * FP * t)
    pumps = [np.sin(2 * np.pi * FPUMP * t) * 10 ** (l / 20) for l in LEVELS] + [0 * t]
    jobs = [dict(x=(p + s * probe).astype(np.float32), norm={**tgp.CHAR_ONLY, 'Character': amt}) for p in pumps for s in (1, -1)]
    gp, g16 = band_gain(FPUMP), band_gain(FP)
    for who in ([only] if only else ('ref', 'model')):
        y = tgp.render_many([dict(j, tune=tune) for j in jobs] if who == 'model' else jobs, who, cache=who == 'ref')
        pr = lambda i: tone(0.5 * (y[2 * i][0] - y[2 * i + 1][0]), FP)
        ev = lambda i, f: abs(tone(0.5 * (y[2 * i][0] + y[2 * i + 1][0]), f))
        node = int(round(amt * 16)); share = high_share(FP, node)
        p0 = pr(len(LEVELS)); M = 10 ** (I.GAINS16[2][node] / 20)
        print(f'{who}: probe alone {20 * np.log10(abs(p0) / PROBE):+.3f} dB, high share {abs(share):.4f}')
        for i, l in enumerate(LEVELS):
            A = 10 ** (l / 20) * gp
            P = (1 + (pr(i) - p0) / (p0 * share)).real; H3 = ev(i, 3 * FPUMP) / M
            try: n, k = solve(P, H3, A)
            except ValueError: n, k = float('nan'), float('nan')
            print(f'  {l:4d} dBFS   probe {20 * np.log10(P):+7.3f} dB   H3 {20 * np.log10(H3):+7.2f} dB   '
                  f'n {20 * np.log10(n):+7.3f} dB   k {k:.4f}', flush=True)
