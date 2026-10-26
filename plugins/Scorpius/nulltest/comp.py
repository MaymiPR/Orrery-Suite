"""Compression (question 5), amount 1 (the wet path alone, since the amount is an exact wet / dry mix):
static curve per mode with 1 kHz tones, detector frequency weighting, peak or RMS, stereo link, attack and release
from level steps. Writes measurements/scorpius/comp.json.
    python comp.py"""
import json, os
import numpy as np
import orion as o

OUT = os.path.join(o.ROOT, 'measurements', 'scorpius'); os.makedirs(OUT, exist_ok=True)
MODES = {'a': {'ids.compression.mode': 'Mode A'}, 'b': {'ids.compression.mode': 'Mode B'}}
P = {**o.ONLY('comp'), 'Comp Amount': 1}


def gain(y, x, a=24000, b=None):
    return o.rms_db(y[a:b]) - o.rms_db(x[a:b])


def static(mode):
    levels = list(range(-40, 13, 2))
    ys = o.render_many([dict(x=o.tone(1000, 1.5, lv), params=P, hidden=MODES[mode]) for lv in levels], cache=True)
    return {lv: gain(y[0], o.tone(1000, 1.5, lv)) for lv, y in zip(levels, ys)}


def weighting(mode):
    """gain reduction of tones at -6 dBFS across frequency (the make-up taken from a quiet tone of each)"""
    res = {}
    for f in (30, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 15000):
        loud, quiet = o.render_many([dict(x=o.tone(f, 1.5, -6), params=P, hidden=MODES[mode]),
                                     dict(x=o.tone(f, 1.5, -40), params=P, hidden=MODES[mode])], cache=True)
        res[f] = gain(loud[0], o.tone(f, 1.5, -6)) - gain(quiet[0], o.tone(f, 1.5, -40))
    return res


def detector(mode):
    """a tone and a noise of the same RMS, then of the same peak; one channel loud and the other quiet"""
    t = o.tone(1000, 2, -10)
    n = o.noise(2, -10, seed=4)
    npk = n * (np.abs(t).max() / np.abs(n).max())
    yt, yn, yp = o.render_many([dict(x=v, params=P, hidden=MODES[mode]) for v in (t, n, npk)], cache=True)
    st = np.stack([o.tone(1000, 2, -6), o.tone(1000, 2, -40)])
    ys = o.render(st, params=P, hidden=MODES[mode], cache=True)
    q = o.render(np.stack([o.tone(1000, 2, -40)] * 2), params=P, hidden=MODES[mode], cache=True)
    return dict(tone_rms=gain(yt[0], t), noise_same_rms=gain(yn[0], n), noise_same_peak=gain(yp[0], npk),
                quiet_channel_beside_loud=gain(ys[1], st[1]), quiet_alone=gain(q[1], st[1]))


def steps(mode):
    """1 kHz at -30 dBFS, a step to -6 for 1 s, back to -30: the gain every ms around both steps"""
    fs = o.FS
    t = np.arange(4 * fs) / fs
    lev = np.where((t >= 1) & (t < 2), 10 ** (-6 / 20), 10 ** (-30 / 20))
    x = (lev * np.sin(2 * np.pi * 1000 * t)).astype(np.float32)
    y = o.render(x, params=P, hidden=MODES[mode], cache=True)[0]
    def track(a, b):
        return [o.rms_db(y[i:i + 48]) - o.rms_db(x[i:i + 48]) for i in range(a, b, 48)]
    return dict(attack=track(fs - 480, fs + 48 * 300), release=track(2 * fs - 480, 2 * fs + 48 * 2000))


if __name__ == '__main__':
    res = {}
    for m in MODES:
        res[m] = dict(static=static(m), weighting=weighting(m), detector=detector(m), steps=steps(m))
        s = res[m]['static']
        print(f'mode {m} static gain: ' + ' '.join(f'{k}:{v:+.1f}' for k, v in s.items()))
        print(f'       reduction at -6 dBFS by frequency: ' + ' '.join(f'{k}:{v:+.1f}' for k, v in res[m]['weighting'].items()))
        print(f'       detector: ' + ' '.join(f'{k} {v:+.2f}' for k, v in res[m]['detector'].items()))
        a = res[m]['steps']['attack']; r = res[m]['steps']['release']
        print(f'       attack (ms from -10): ' + ' '.join('%.1f' % v for v in a[:60:2]))
        print(f'       release (every 20 ms from -10): ' + ' '.join('%.1f' % v for v in r[:2000:20]))
    json.dump(res, open(os.path.join(OUT, 'comp.json'), 'w'), indent=1)
