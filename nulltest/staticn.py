"""The high band's steady gain n against level, read through the shaper measured with anti-phase tones (shapeside.py:
y = M (d u + (1 - d) sin(k u) / k), u = n s): a staircase of tones (1 dB steps, 1.5 s each), n solved per step over
its last half second. Reference against ours; tone, two tones and band noise (a peak detector reads them apart).

    python staticn.py [amount 0..1] [freq Hz]          (the in-phase shaper includes the mid gain, shapemid.py)"""
import os, sys
import numpy as np
from scipy.optimize import minimize_scalar
import tgp, ident as I

FS = tgp.FS
LEVELS = list(range(-24, 7))
SECS = 1.5
D = os.path.join(tgp.ROOT, 'measurements', 'character')


def shaper(amt):
    for l in open(os.path.join(D, 'shaper_high.txt')):
        f = l.split()
        if abs(float(f[2]) - amt) < 1e-6: return float(f[3]), float(f[4])


def staircase(kind, f0, seed=3):
    n = int(SECS * FS); t = np.arange(n * len(LEVELS)) / FS
    if kind == 'tone': w = np.sin(2 * np.pi * f0 * t)
    elif kind == 'twotone': w = (np.sin(2 * np.pi * f0 * 0.8 * t) + np.sin(2 * np.pi * f0 * 1.25 * t)) / np.sqrt(2)
    else:
        from scipy.signal import butter, sosfiltfilt
        w = sosfiltfilt(butter(6, (5000, 11000), 'band', fs=FS, output='sos'), np.random.default_rng(seed).standard_normal(len(t)))
        w /= np.sqrt(2 * np.mean(w ** 2))
    g = np.repeat([10 ** (l / 20) for l in LEVELS], n)
    return np.concatenate([np.zeros(I.PRE), w * g]), n


def gamma(amt):
    """the mid output's extra distortion share (shapemid.py, 1 + gamma at the row)"""
    row = int(round(amt * 16))
    for l in open(os.path.join(D, 'shapemid_high.txt')):
        f = l.split()
        if f[0] == 'SHAPEMID' and int(f[1]) == row: return float(f[2]) - 1
    return 0.0


def solve_n(y, s, k, d, M, g=0.0):
    """n from the band's in-phase output: u + (1 + g)(1 - d)(sin(k u) / k - u), u = n s"""
    F = lambda n: M * (n * s + (1 + g) * (1 - d) * (np.sin(k * n * s) / k - n * s))
    return minimize_scalar(lambda n: np.sum((y - F(n)) ** 2), bounds=(0.05, 1.5), method='bounded', options=dict(xatol=1e-9)).x


if __name__ == '__main__':
    amt = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5
    f0 = float(sys.argv[2]) if len(sys.argv) > 2 else 8000.0
    node = int(round(amt * 16)); k, d = shaper(amt)
    M = [10 ** (I.GAINS16[b][node] / 20) for b in range(3)]
    res = {}
    for kind in ('tone', 'twotone', 'noise'):
        x, n = staircase(kind, f0)
        xb = I.bands(x)[:, 0]; nl = I.nl_bands(x)[:, 0]
        j = dict(x=np.stack([x, x]).astype(np.float32), norm={**tgp.CHAR_ONLY, 'Character': amt}, raw=True)
        for who in ('ref', 'model'):
            y = tgp.render_many([j], who, cache=who == 'ref')[0][0]
            yh = y - M[0] * xb[0] - M[1] * xb[1] - M[2] * (xb[2] - nl[2])
            ns = []
            for i in range(len(LEVELS)):
                s = slice(I.PRE + i * n + n * 2 // 3, I.PRE + (i + 1) * n)
                ns.append(solve_n(yh[s], nl[2][s], k, d, M[2], gamma(amt)))
            res[kind, who] = 20 * np.log10(np.array(ns))
        print(f'{kind:8s} level: ' + ' '.join(f'{l:+6d}' for l in LEVELS))
        print(f'{"":8s} ref n: ' + ' '.join(f'{v:+6.2f}' for v in res[kind, 'ref']))
        print(f'{"":8s} ours-ref' + ' '.join(f'{v:+6.3f}' for v in res[kind, 'model'] - res[kind, 'ref']), flush=True)
    np.save(os.path.join(D, f'staticn_{amt}_{int(f0)}.npy'), np.array([[res[k_, w] for w in ('ref', 'model')] for k_ in ('tone', 'twotone', 'noise')]))
