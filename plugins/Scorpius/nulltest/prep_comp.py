"""Compression's wet path as gain -> colour (REA notes 2026-10-28: the reference ships 1-layer LSTM captures named for
a compressor's A / B colour; comp_colour.py finds even harmonics in the wet path below the threshold, where the gain
stands still, so it is more than a gain and a linear filter). Puts our fitted gain computer (Compressor.h, Tuning.h
constants, the same arithmetic) in front: u = x g(x), and writes <case>_u.npz for train_wn.py, with a linear
estimate of the wet filter (Wiener, 8192 taps) as the Post filter's start, and the linear-only null on the held-out
pieces as the baseline.
    python prep_comp.py [a|b ...]
Also writes <case>_r.npz: the same input, the target less what the fitted wet IIR (LinearFits.h compWetA / B, the
plug-in's cheap linear part) makes of it, for a small network that learns only the rest."""
import re
import os, sys
import numpy as np
from scipy.signal import fftconvolve
import orion as o
from train_wh import NN, IR

FS = 48000
TUNE = {'a': dict(T=-12.818, R=4.987, W=7.391, att=1.027, rel=19.330, rms=0.0536),
        'b': dict(T=-12.827, R=4.969, W=7.341, att=1.022, rel=19.263, rms=0.050)}


def gain(x, T, R, W, att, rel, rms):
    """Compressor.h's gain for a mono input (mid = x), per sample, linear"""
    a_ = np.exp(-1.0 / (att * 1e-3 * FS)); r_ = np.exp(-1.0 / (rel * 1e-3 * FS))
    env = envSq = 0.0
    out = np.empty(len(x))
    s = 1 - 1 / R; lg = np.log10
    for i, v in enumerate(x.tolist()):
        a = abs(v)
        env = a + (env - a) * (a_ if a > env else r_)
        q = v * v
        envSq = q + (envSq - q) * (a_ if q > envSq else r_)
        level = (1 - rms) * env + rms * (2.0 * envSq) ** 0.5
        over = 20.0 * lg(max(level, 1e-9)) - T
        if over >= W / 2: g = s * over
        elif over > -W / 2: g = s * (over + W / 2) ** 2 / (2 * W)
        else: g = 0.0
        out[i] = 10 ** (-g / 20)
    return out


def iir(name):
    """a ParallelFilter of LinearFits.h as a function: d x + sum (b0 + b1 z^-1) / (1 + a1 z^-1 + a2 z^-2) x"""
    from scipy.signal import lfilter
    src = open(os.path.join(o.PLUGIN, 'Source', 'dsp', 'LinearFits.h')).read()
    line = re.search(r'inline const Parallel ' + name + r' \{ ([^;]*)\};', src).group(1)
    nums = [float(v) for v in re.findall(r'[-+]?\d[\d.]*(?:e[-+]?\d+)?', line)]
    d, secs = nums[0], np.array(nums[1:]).reshape(-1, 4)
    return lambda x: d * x + sum(lfilter([s[0], s[1]], [1, s[2], s[3]], x) for s in secs)


def wiener(us, ys, n=8192, nfft=32768):
    Sxy = np.zeros(nfft // 2 + 1, complex); Sxx = np.zeros(nfft // 2 + 1)
    for u, y in zip(us, ys):
        for s in range(0, len(u) - nfft, nfft // 4):
            U = np.fft.rfft(u[s:s + nfft]); Y = np.fft.rfft(y[s:s + nfft])
            Sxy += np.conj(U) * Y; Sxx += np.abs(U) ** 2
    h = np.fft.irfft(Sxy / (Sxx + 1e-12), nfft)[:n]
    return (h * np.r_[np.ones(n - 512), np.hanning(1024)[512:]]).astype(np.float32)


if __name__ == '__main__':
    for m in sys.argv[1:] or ['a', 'b']:
        d = np.load(os.path.join(NN, f'comp_{m}.npz'), allow_pickle=True)
        xs, ys, valid = list(d['x']), list(d['y']), list(d['valid'])
        us = [(x * gain(x.astype(np.float64), **TUNE[m])).astype(np.float32) for x in xs]
        tr = [i for i, v in enumerate(valid) if not v and np.abs(xs[i]).max() <= 10 ** (4 / 20)]
        h = wiener([us[i].astype(np.float64) for i in tr], [ys[i].astype(np.float64) for i in tr])
        np.save(os.path.join(IR, f'comp_{m}.npy'), h[None])
        nulls = [o.null_db(ys[i][None], fftconvolve(us[i], h)[:len(us[i])][None]) for i, v in enumerate(valid) if v and np.abs(xs[i]).max() <= 10 ** (4 / 20)]
        print(f'comp_{m}: linear (gain + Wiener filter) held-out nulls', ' '.join('%.1f' % v for v in nulls), 'mean %.1f' % np.mean(nulls), flush=True)
        np.savez_compressed(os.path.join(NN, f'comp_{m}_u.npz'), x=np.array(us, dtype=object), y=d['y'], kind=d['kind'], valid=d['valid'], xraw=d['x'])
        lin = iir('compWet' + m.upper())
        rs = [(y.astype(np.float64) - lin(u.astype(np.float64))).astype(np.float32) for u, y in zip(us, ys)]
        nulls = [o.null_db(ys[i][None], (ys[i] - rs[i])[None]) for i, v in enumerate(valid) if v and np.abs(xs[i]).max() <= 10 ** (4 / 20)]
        print(f'comp_{m}: linear (gain + fitted IIR) held-out nulls', ' '.join('%.1f' % v for v in nulls), 'mean %.1f' % np.mean(nulls), flush=True)
        np.savez_compressed(os.path.join(NN, f'comp_{m}_r.npz'), x=np.array(us, dtype=object), y=np.array(rs, dtype=object), kind=d['kind'], valid=d['valid'])
