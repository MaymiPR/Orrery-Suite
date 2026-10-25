"""Parameter smoothing: a quiet steady tone (below every threshold) while one parameter jumps at a block boundary
(block 64; the change lands at sample 2 s after the pre-roll). The gain on the tone vs time (complex demodulation,
1 ms window) re the same render without the change shows each parameter's smoothing law and time.
Writes measurements/smoothing/smoothing.json and a plot."""
import json, os
import numpy as np
import tgp, plotstyle as ps

OUT = os.path.join(tgp.ROOT, 'measurements', 'smoothing'); os.makedirs(OUT, exist_ok=True)
FS = tgp.FS
PRE = 2 * FS
AT = PRE + 2 * FS
DUR = 5.0
LIM_DEFAULT = 0.494206

# name, stage preset, fixed params (normalised), automated parameter, from, to, tone Hz, tone dBFS
CASES = [
    ('Input Gain 0 -> +12 dB', tgp.NONE, {}, 'Input Gain', 0.5, 1.0, 1000, -40),
    ('Output Gain 0 -> -12 dB', tgp.NONE, {}, 'Output Gain', 0.5, 0.0, 1000, -40),
    ('EQ Mid Gain 0 -> +6 dB', tgp.EQ_ONLY, {}, 'EQ Mid Gain', 0.5, 1.0, 637, -40),
    ('EQ Low Gain 0 -> -6 dB', tgp.EQ_ONLY, {}, 'EQ Low Gain', 0.5, 0.0, 60, -40),
    ('Character 100 -> 200 %', tgp.CHAR_ONLY, {}, 'Character', 0.5, 1.0, 1000, -60),
    ('Character 100 -> 0 %', tgp.CHAR_ONLY, {}, 'Character', 0.5, 0.0, 1000, -60),
    ('Limiter Input Gain 5 -> 12 dB', tgp.LIM_ONLY, {}, 'Limiter Input Gain', LIM_DEFAULT, 1.0, 1000, -40),
    ('EQ Bypass on (EQ Mid +6)', {'Character Bypass': 1, 'Limiter Bypass': 1, 'EQ Mid Gain': 1.0}, {}, 'EQ Bypass', 0.0, 1.0, 637, -40),
    ('Character Bypass on (100 %)', tgp.CHAR_ONLY, {}, 'Character Bypass', 0.0, 1.0, 1000, -60),
    ('Limiter Bypass on (5 dB)', tgp.LIM_ONLY, {}, 'Limiter Bypass', 0.0, 1.0, 1000, -40),
    ('Bypass on (defaults)', {}, {}, 'Bypass', 0.0, 1.0, 1000, -60),
]


def demod(y, f):
    t = np.arange(len(y)) / FS
    k = np.hanning(49); k /= k.sum()
    return np.convolve(y * np.exp(-2j * np.pi * f * t), k, 'same')


if __name__ == '__main__':
    jobs = []
    for name, preset, fixed, p, a, b, f, l in CASES:
        x = np.concatenate([np.zeros(PRE), tgp.tone(f, DUR, l)])
        norm = {**preset, **fixed, p: a}
        jobs.append(dict(x=x, norm=norm, raw=True))
        jobs.append(dict(x=x, norm=norm, auto=[(p, AT, b)], raw=True))
    ys = tgp.render_many(jobs, block=64, cache=True)
    res = {}
    fg, ax = ps.fig('Parameter smoothing: gain change on a steady tone after a jump', 'ms after the change', 'dB re final change', size=(10, 5))
    for i, (name, preset, fixed, p, a, b, f, l) in enumerate(CASES):
        z0 = demod(ys[2 * i][0], f); z1 = demod(ys[2 * i + 1][0], f)
        g = 20 * np.log10(np.abs(z1) / np.abs(z0))
        ph = np.angle(z1 / z0)
        seg = g[AT - 48 * 20: AT + 48 * 1000]
        final = float(np.median(g[AT + 48 * 800: AT + 48 * 1000]))
        start = float(np.median(g[AT - 48 * 100: AT - 48 * 20]))
        span = final - start
        rel = (g - start) / span if abs(span) > 1e-3 else g * 0
        idx = lambda q: next((k for k in range(AT - 48 * 20, AT + 48 * 1000) if rel[k] >= q), None)
        t10, t50, t90, t99 = (idx(q) for q in (0.1, 0.5, 0.9, 0.99))
        ms = lambda k: None if k is None else round((k - AT) / 48, 2)
        res[name] = dict(change_dB=round(span, 3), t10_ms=ms(t10), t50_ms=ms(t50), t90_ms=ms(t90), t99_ms=ms(t99),
                         phase_change_deg=round(float(np.degrees(np.median(ph[AT + 48 * 800: AT + 48 * 1000]))), 2),
                         curve_ms=np.round(rel[AT - 48 * 20: AT + 48 * 300: 12], 4).tolist())
        print(name, {k: v for k, v in res[name].items() if k != 'curve_ms'})
        if abs(span) > 0.05:
            ax.plot((np.arange(len(seg)) - 48 * 20) / 48, rel[AT - 48 * 20: AT + 48 * 1000], label=name, lw=1.3)
    ax.set_xlim(-20, 300)
    ps.save(fg, os.path.join(OUT, 'smoothing.png'))
    json.dump(res, open(os.path.join(OUT, 'smoothing.json'), 'w'), indent=1)
