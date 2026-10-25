"""Character section, small-signal response vs amount: an impulse at -60 dBFS (below every threshold) through the
character stage only, divided by the all-bypassed path. Each response is fitted with the 3-band LR4 model from
fit_crossover.py (crossovers fixed, one real gain per band), which gives the per-band make-up gain vs amount and shows
what the band model leaves unexplained. Writes measurements/character/small_signal.json and a plot."""
import json, os
import numpy as np
from scipy.optimize import least_squares
import tgp, plotstyle as ps

OUT = os.path.join(tgp.ROOT, 'measurements', 'character')
FS = tgp.FS
PRE = 2 * FS; N = 65536
AMOUNTS = [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0]
XO = json.load(open(os.path.join(OUT, 'crossover_fit.json')))['LR4']
f = np.fft.rfftfreq(N, 1 / FS)
sel = (f > 15) & (f < 22000)


def lr4(fc):
    s = 1j * f / fc; b = 1 / (s ** 2 + np.sqrt(2) * s + 1)
    return b * b, (s ** 2 * b) ** 2


l1, h1 = lr4(XO['fc1_hz']); l2, h2 = lr4(XO['fc2_hz'])
BANDS = [l1 * (l2 + h2), h1 * l2, h1 * h2]
ALL = BANDS[0] + BANDS[1] + BANDS[2]

if __name__ == '__main__':
    x = np.zeros(PRE + N, np.float32); x[PRE] = 10 ** (-60 / 20)
    jobs = [dict(x=x, norm=tgp.NONE, raw=True)] + [dict(x=x, norm={**tgp.CHAR_ONLY, 'Character': a}, raw=True) for a in AMOUNTS]
    ys = tgp.render_many(jobs, cache=True)
    H0 = np.fft.rfft(ys[0][0, PRE:PRE + N])
    res = dict(amounts_pct=[a * 200 for a in AMOUNTS], fit={}, freq_hz=f[sel][::16].round(2).tolist(), measured_dB={}, model_dB={})
    fg, ax = ps.fig('Character, small signal (-60 dBFS impulse) re bypassed path', 'Hz', 'dB', logx=True)
    for a, y in zip(AMOUNTS, ys[1:]):
        H = np.fft.rfft(y[0, PRE:PRE + N]) / H0
        def model(p): return (sum(10 ** (p[i] / 20) * BANDS[i] for i in range(3)) / ALL) * np.exp(-2j * np.pi * f * p[3] / FS)
        def resid(p):
            e = model(p)[sel] / H[sel]
            return np.concatenate([20 * np.log10(np.abs(e)), np.angle(e) * 8.686])
        r = least_squares(resid, [0, 0, 0, 0])
        m = model(r.x)
        mag = 20 * np.log10(np.abs(H[sel])); mm = 20 * np.log10(np.abs(m[sel]))
        res['fit'][str(a * 200)] = dict(low_dB=round(r.x[0], 3), mid_dB=round(r.x[1], 3), high_dB=round(r.x[2], 3), extra_delay_samples=round(r.x[3], 3),
                                       max_mag_err_dB=round(float(np.max(np.abs(mag - mm))), 3),
                                       mag_err_at_hz={str(q): round(float(np.interp(q, f[sel], mag - mm)), 3) for q in (30, 100, 300, 600, 1000, 2000, 5000, 10000, 16000)})
        res['measured_dB'][str(a * 200)] = mag[::16].round(3).tolist(); res['model_dB'][str(a * 200)] = mm[::16].round(3).tolist()
        print(f'{a * 200:5.0f}%', res['fit'][str(a * 200)])
        if a in (0.25, 0.5, 0.75, 1.0):
            l, = ax.plot(f[sel], mag, label=f'{a * 200:.0f} %')
            ax.plot(f[sel], mm, color=l.get_color(), ls=':', lw=1.2, label='_')
    ax.set_xlim(15, 22000)
    ps.save(fg, os.path.join(OUT, 'small_signal.png'))
    json.dump(res, open(os.path.join(OUT, 'small_signal.json'), 'w'), indent=1)
