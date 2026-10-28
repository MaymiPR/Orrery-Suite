"""Fits our oversampling filters (Source/dsp/Oversampler.h designOsStage) to the reference's measured path (cached
renders of measure_oversampling.py, 48 kHz). Each stage k runs at 2^(k+1) times the host rate and is a
Kaiser-windowed sinc cut at a quarter of that rate, its length and window from the textbook Kaiser formulas for a
transition band between a passband edge in Hz and its mirror image and a stopband depth; unit gain at DC (the
structure is from the binary notes, reference/rea/; the two numbers are fitted here). The path simulated is
up-sampling stages, the fixed delay with its fraction at the high rate, down-sampling stages. Result: depth 100 dB,
edge 16.7 ... 17.1 kHz at 48 kHz (45 / 21 / 17 taps), narrowed to 16.9 ... 17.0 by 44.1 kHz (os_rates_fit.py, which
also gives the per-rate delays in Engine.cpp).
    python fit_oversampling.py [edgeHz depthDb]"""
import os, sys
import numpy as np
from scipy.signal import lfilter
import tgp

FS = tgp.FS
PRE = 2 * FS
N = 4096
CORE = 151.27917                                  # the fixed delay at 48 kHz (Tuning.h coreDelay)
EXTRA = {2: 22.488, 4: 27.711, 8: 29.655}         # the path's extra delay (group delay at 1 kHz)


def design(k, fs, edge, depth):
    rate = fs * 2 ** (k + 1)
    p = min(edge, 0.225 * rate); s = rate / 2 - p
    tw = (s - p) / rate
    a = -depth
    order = int(np.ceil((a - 7.95) / (2.285 * tw * 2 * np.pi)))
    order += order % 2
    beta = 0.1102 * (a - 8.7) if a > 50 else 0.5842 * (a - 21) ** 0.4 + 0.07886 * (a - 21)
    n = np.arange(order + 1) - order / 2
    h = 0.5 * np.sinc(0.5 * n) * np.kaiser(order + 1, beta)
    return h / h.sum()


def simulate(stages, k, fs=FS):
    x = np.zeros(N); x[0] = 1.0
    for h in stages:
        z = np.zeros(2 * len(x)); z[0::2] = x
        x = 2 * np.convolve(z, h)[:len(z)]
    lat = sum((len(h) - 1) / (2 << i) for i, h in enumerate(stages))
    d = (CORE + EXTRA[k] - lat) * (2 ** len(stages))
    whole = int(np.floor(d)); frac = d - whole
    if frac < 0.05 and whole > 0: frac += 1; whole -= 1
    a = (1 - frac) / (1 + frac)
    x = lfilter([a, 1], [1, a], np.concatenate([np.zeros(whole), x])[:len(x)])
    for h in stages[::-1]:
        x = np.convolve(x, h)[:len(x)][0::2]
    return x[:N]


def refs():
    imp = np.zeros(PRE + 32768, np.float32); imp[PRE] = 1e-3
    jobs = [dict(x=imp, norm=tgp.NONE, raw=True, salt=f'os{k}') for k in (2, 4, 8)]
    return {k: np.fft.rfft(y[0, PRE:PRE + N] / 1e-3) for k, y in zip((2, 4, 8), tgp.render_many(jobs, cache=True))}


def report(R, edge, depth, quiet=False):
    f = np.fft.rfftfreq(N, 1 / FS); sel = (f > 20) & (f < 23900)
    tot = 0
    for k in (2, 4, 8):
        st = [design(i, FS, edge, depth) for i in range({2: 1, 4: 2, 8: 3}[k])]
        e = np.fft.rfft(simulate(st, k)) / R[k]
        m = np.max(np.abs(20 * np.log10(np.abs(e[sel])))); tot += m
        if not quiet:
            print(f'{k}x taps {[len(h) for h in st]}: max {m:.4f} dB, {np.max(np.abs(np.degrees(np.angle(e[sel])))):.2f} deg;',
                  'at 20/22/23 kHz', [round(float(20 * np.log10(abs(np.interp(q, f, e.real) + 1j * np.interp(q, f, e.imag)))), 4) for q in (20000, 22000, 23000)])
    return tot


if __name__ == '__main__':
    R = refs()
    if len(sys.argv) > 2:
        report(R, float(sys.argv[1]), float(sys.argv[2])); raise SystemExit
    best = min(((report(R, e, d, True), e, d) for e in range(14000, 20001, 250) for d in range(-120, -59, 5)))
    print('grid best', best)
    _, e0, d0 = best
    best = min(((report(R, e, d, True), e, d) for e in np.arange(e0 - 250, e0 + 251, 25) for d in np.arange(d0 - 5, d0 + 5.1, 0.5)))
    print('fine best', best)
    report(R, best[1], best[2])
