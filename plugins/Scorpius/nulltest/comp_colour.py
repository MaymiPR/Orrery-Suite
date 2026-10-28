"""Is compression's wet path more than a gain and a linear filter? (REA notes, 2026-10-28: Orion ships two 1-layer
LSTM captures named for a compressor's A / B colour "with no compression"; the hypothesis, unverified, is that modes
a / b colour the wet signal with them.) Steady tones below the threshold (the gain then stands still): any harmonics
in the wet path come from something nonlinear besides the gain. Compression alone, amount 1, both modes.
    python comp_colour.py"""
import numpy as np
import orion as o

MODES = {'a': {'ids.compression.mode': 'Mode A'}, 'b': {'ids.compression.mode': 'Mode B'}}
P = {**o.ONLY('comp'), 'Comp Amount': 1}
P0 = {**o.ONLY('comp'), 'Comp Amount': 0}


def harmonics(y, f, n=5):
    """levels of harmonics 2 ... n re the fundamental (dB), from 1 s of steady output (Blackman-Harris window)"""
    seg = y[o.FS // 2: o.FS // 2 + o.FS].astype(np.float64)
    w = np.blackman(len(seg))
    s = np.abs(np.fft.rfft(seg * w))
    def at(k):
        b = int(round(k * f * len(seg) / o.FS))
        return s[max(0, b - 3): b + 4].max() if b + 4 <= len(s) else 1e-30   # (above Nyquist: none)
    f1 = at(1)
    return [20 * np.log10(at(k) / f1 + 1e-15) for k in range(2, n + 1)]


if __name__ == '__main__':
    jobs, keys = [], []
    for f in (100, 1000, 5000):
        for lv in (-36, -24, -18):
            for m in ('a', 'b', 'off'):
                params = P0 if m == 'off' else P
                jobs.append(dict(x=o.tone(f, 2, lv), params=params, hidden=MODES['a' if m == 'off' else m]))
                keys.append((f, lv, m))
    ys = o.render_many(jobs, cache=True)
    print('freq level mode : h2 h3 h4 h5 (dB re fundamental)')
    for (f, lv, m), y in zip(keys, ys):
        print(f'{f:5d} {lv:4d} {m:3s} :', ' '.join(f'{h:7.1f}' for h in harmonics(y[0], f)))
