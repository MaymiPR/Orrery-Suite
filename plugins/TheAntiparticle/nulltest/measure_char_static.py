"""Character section (EQ and limiter bypassed): steady-state gain vs input level for sines in each region, at five
amount settings. A staircase per (frequency, amount): 1.2 s per level, gain read over the last 0.4 s of each step
(lock-in at the tone frequency). Also runs the staircase downwards (hysteresis / release) at the default amount.
Writes measurements/character/static.json."""
import json, os, sys
import numpy as np
import tgp

OUT = os.path.join(tgp.ROOT, 'measurements', 'character'); os.makedirs(OUT, exist_ok=True)
FS = tgp.FS
STEP = 1.2
LEVELS = list(range(-80, 7, 3))
FREQS = [40, 100, 300, 1000, 3000, 8000, 15000]
AMOUNTS = [0.0, 0.25, 0.5, 0.75, 1.0]
LEAD = 1.0


def staircase(freq, levels):
    t = np.arange(int(STEP * FS)) / FS
    parts = [np.zeros(int(LEAD * FS))]
    for l in levels:
        parts.append(10 ** (l / 20) * np.sin(2 * np.pi * freq * (t + len(parts) * STEP)))
    return np.concatenate(parts).astype(np.float32)


def lockin(y, freq, a, b):
    t = np.arange(a, b) / FS
    c, s = np.cos(2 * np.pi * freq * t), np.sin(2 * np.pi * freq * t)
    seg = y[a:b]
    return 2 * np.hypot(np.mean(seg * c), np.mean(seg * s))


def harmonics(y, freq, levels, n=9):
    """Level of harmonics 2..n re the fundamental (dBc), per step; harmonics above 22 kHz are skipped."""
    out = []
    for i, l in enumerate(levels):
        a = int((LEAD + (i + 1) * STEP - 0.4) * FS); b = int((LEAD + (i + 1) * STEP) * FS)
        f1 = lockin(y, freq, a, b)
        out.append([round(20 * np.log10(max(lockin(y, k * freq, a, b), 1e-12) / max(f1, 1e-12)), 2) for k in range(2, n + 1) if k * freq < 22000])
    return out


def gains(y, freq, levels):
    out = []
    for i, l in enumerate(levels):
        a = int((LEAD + (i + 1) * STEP - 0.4) * FS); b = int((LEAD + (i + 1) * STEP) * FS)
        out.append(20 * np.log10(max(lockin(y, freq, a, b), 1e-12)) - l)
    return out


if __name__ == '__main__':
    jobs, keys = [], []
    for fq in FREQS:
        for am in AMOUNTS:
            jobs.append(dict(x=staircase(fq, LEVELS), norm={**tgp.CHAR_ONLY, 'Character': am})); keys.append((fq, am, 'up'))
        jobs.append(dict(x=staircase(fq, LEVELS[::-1]), norm={**tgp.CHAR_ONLY, 'Character': 0.5})); keys.append((fq, 0.5, 'down'))
    ys = tgp.render_many(jobs, cache=True)
    res = {}
    for (fq, am, d), y in zip(keys, ys):
        lv = LEVELS if d == 'up' else LEVELS[::-1]
        res[f'{fq}|{am}|{d}'] = dict(levels=lv, gain_dB=[round(g, 3) for g in gains(y[0], fq, lv)],
                                     harmonics_dBc=harmonics(y[0], fq, lv))
    json.dump(res, open(os.path.join(OUT, 'static.json'), 'w'), indent=1)
    for fq in FREQS:
        print(f'== {fq} Hz: gain (dB) vs input level')
        print('   level   ' + ' '.join(f'{l:6d}' for l in LEVELS))
        for am in AMOUNTS:
            print(f'   {am * 200:4.0f}%   ' + ' '.join(f'{g:6.2f}' for g in res[f"{fq}|{am}|up"]['gain_dB']))
        dn = dict(zip(res[f'{fq}|0.5|down']['levels'], res[f'{fq}|0.5|down']['gain_dB']))
        print('   100%dn  ' + ' '.join(f'{dn[l]:6.2f}' for l in LEVELS))
