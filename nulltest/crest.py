"""Crest-factor test of a band's detector: a sine, two tones and band-limited noise at the same RMS (and three
levels), settled band gain in the reference and in ours. A mean-square detector gives all three the same gain; one
that reacts to peaks compresses the peakier signals more.

    python crest.py high [amount 0..1]"""
import sys
import numpy as np
from scipy.signal import butter, sosfiltfilt
import tgp, ident as I

FS = tgp.FS; PRE = 2 * FS
CENTRE = {'low': 70.0, 'mid': 700.0, 'high': 8000.0}
NOISE = {'low': (45, 110), 'mid': (400, 1200), 'high': (5000, 11000)}


def signals(band, n, seed=3):
    t = np.arange(n) / FS; f = CENTRE[band]
    rng = np.random.default_rng(seed)
    w = sosfiltfilt(butter(4 if band == 'low' else 6, NOISE[band], 'band', fs=FS, output='sos'), rng.standard_normal(n))
    out = {'sine': np.sin(2 * np.pi * f * t), 'two-tone': np.sin(2 * np.pi * f * 0.88 * t) + np.sin(2 * np.pi * f * 1.13 * t), 'noise': w}
    for k, v in out.items():
        out[k] = v / np.sqrt(2 * np.mean(v[PRE:] ** 2))        # the RMS of a unit-peak sine
        out[k][:PRE // 2] = 0
    return out


if __name__ == '__main__':
    band = sys.argv[1]; amt = float(sys.argv[2]) if len(sys.argv) > 2 else 0.5
    n = PRE + 2 * FS
    sig = signals(band, n)
    bi = ['low', 'mid', 'high'].index(band)
    for lvl in (-20, -10, -4):
        jobs = [dict(x=(v * 10 ** (lvl / 20)).astype(np.float32), norm={**tgp.CHAR_ONLY, 'Character': amt}, raw=True) for v in sig.values()]
        r = tgp.render_many(jobs, 'ref', cache=True); o = tgp.render_many(jobs, 'model')
        row = []
        for (k, v), yr, yo, j in zip(sig.items(), r, o, jobs):
            x = j['x']; seg = slice(PRE + FS // 2, n)
            lin = sum(10 ** (I.GAINS[b][int(round(amt * 8))] / 20) * I.bands(x)[b, 0] for b in range(3))
            gr = 10 * np.log10(np.sum(yr[0][seg] ** 2) / np.sum(lin[seg] ** 2)); go = 10 * np.log10(np.sum(yo[0][seg] ** 2) / np.sum(lin[seg] ** 2))
            nul = 10 * np.log10(np.sum((yr[0][seg] - yo[0][seg]) ** 2) / np.sum(yr[0][seg] ** 2))
            row.append(f'{k}: ref {gr:+.3f} ours {go:+.3f} (null {nul:.1f})')
        print(f'{band} {amt*200:.0f}% {lvl:+d} dB  ' + '   '.join(row), flush=True)
