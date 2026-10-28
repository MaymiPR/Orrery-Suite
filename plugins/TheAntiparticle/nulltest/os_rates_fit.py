"""fit_oversampling.py's design checked at 44.1 / 88.2 / 96 kHz against measure_os_rates.py's renders: per rate and ratio
the whole path's delay is fitted (the fixed delay sits inside the oversampled part), then the magnitude/phase error
below 0.497 fs is reported for a range of passband edges."""
import sys
import numpy as np
from scipy.optimize import minimize_scalar
from scipy.signal import lfilter
import tgp
from fit_oversampling import design
from measure_os_rates import jobs, RATES

N = 8192


def sim(stages, d_total):
    x = np.zeros(N); x[0] = 1.0
    for h in stages:
        z = np.zeros(2 * len(x)); z[0::2] = x
        x = 2 * np.convolve(z, h)[:len(z)]
    lat = sum((len(h) - 1) / (2 << i) for i, h in enumerate(stages))
    d = (d_total - lat) * 2 ** len(stages)
    whole = int(np.floor(d)); frac = d - whole
    if frac < 0.05 and whole > 0: frac += 1; whole -= 1
    a = (1 - frac) / (1 + frac)
    x = lfilter([a, 1], [1, a], np.concatenate([np.zeros(whole), x])[:len(x)])
    for h in stages[::-1]:
        x = np.convolve(x, h)[:len(x)][0::2]
    return x


def err(fs, k, edge, R, depth=-100, verbose=False):
    st = [design(i, fs, edge, depth) for i in range({2: 1, 4: 2, 8: 3}[k])]
    f = np.fft.rfftfreq(N, 1 / fs); sel = (f > 20) & (f < 0.497 * fs)
    peak = np.argmax(np.abs(np.fft.irfft(R)))
    def cost(d):
        e = np.fft.rfft(sim(st, d)) / R
        return np.sum(np.abs(e[sel] - 1) ** 2)
    d = minimize_scalar(cost, bounds=(peak - 2, peak + 2), method='bounded', options=dict(xatol=1e-5)).x
    e = np.fft.rfft(sim(st, d)) / R
    m = np.max(np.abs(20 * np.log10(np.abs(e[sel])))); ph = np.max(np.abs(np.degrees(np.angle(e[sel]))))
    if verbose: print(f'{fs} {k}x taps {[len(h) for h in st]} delay {d:.4f}: max {m:.4f} dB {ph:.2f} deg')
    return m, d


if __name__ == '__main__':
    R = {}
    for fs in RATES:
        for k in (2, 4, 8):
            y = tgp.render_many(jobs(fs, k), fs=fs, cache=True)[0][0, 2 * fs:2 * fs + N] / 1e-3
            R[fs, k] = np.fft.rfft(y)
    edges = [float(a) for a in sys.argv[1:]] or [16500, 16700, 16900, 17000, 17100, 17200]
    for e in edges:
        print('edge', e, ' '.join(f'{fs // 100 / 10}k/{k}x:{err(fs, k, e, R[fs, k])[0]:.3f}' for fs in RATES for k in (2, 4, 8)))
