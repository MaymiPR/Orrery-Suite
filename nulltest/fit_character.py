"""Fits the character section's per-band constants (Source/dsp/Tuning.h, BandParam) to the reference, one band and
one amount node (0, 25, ..., 200 %) at a time, through the character stage alone: a tone in the middle of the band,
stepped through levels (static curve, shaper) and burst on and off (attack, release), plus pink noise and a dense
synthetic mix at two levels each (a tone alone over-fits: the low band's detector came out right for the tone and
wrong for music). Minimises the mean waveform residual (reference - ours) with Nelder-Mead from the current table.
Writes
measurements/character/fit_char.json and Source/dsp/CharacterTable.h (the plug-in's defaults).

    python fit_character.py                    # every band and node
    python fit_character.py mid 4              # one band / node (prints the residual before and after)"""
import json, os, sys
import numpy as np
from scipy.optimize import minimize
import tgp

FS = tgp.FS
PRE = 2 * FS
BANDS = {'low': 70.0, 'mid': 700.0, 'high': 8000.0}
FREE = ['mix', 'cubic', 'thresh', 'ratio', 'knee', 'attack', 'release']   # make-up: the identified small-signal gain
OUT_JSON = os.environ.get('FIT_CHAR_JSON', os.path.join(tgp.ROOT, 'measurements', 'character', 'fit_char.json'))
HEADER = os.path.join(tgp.ROOT, 'Source', 'dsp', 'CharacterTable.h')
DEFAULTS = {'low': dict(mix=0.7, cubic=0.9, thresh=-30, ratio=2.0, knee=20, attack=10, release=300, gainAttack=5, gainRelease=50, attack2=200, release2=3000, slowOffset=-3),
            'mid': dict(mix=0.79, cubic=0.87, thresh=-23, ratio=1.7, knee=35, attack=190, release=90, gainAttack=5, gainRelease=50, attack2=200, release2=3000, slowOffset=-3),
            'high': dict(mix=0.8, cubic=0.0, thresh=-13, ratio=1.8, knee=1, attack=1.5, release=170, gainAttack=5, gainRelease=50, attack2=200, release2=3000, slowOffset=-3)}


# measured small-signal gains (MEASUREMENTS.md 3.2): the make-up starting points
MAKEUP = {'low': [0, 0.7538, 1.4727, 2.1601, 2.8187, 3.155, 3.4787, 3.7908, 4.0921], 'mid': [0, 1.1054, 2.0517, 2.874, 3.5965, 4.3103, 4.97, 5.5831, 6.1557],
          'high': [0, 0.3619, 0.6289, 0.8093, 0.9082, 1.4358, 1.9333, 2.4037, 2.85]}   # Source/dsp/BandFilters.h


def signal(f):
    """level staircase (-50 .. +6 dBFS, 0.5 s steps), then bursts (-40 -> -20 / -6 / 0 -> -40)"""
    parts, t0 = [], 0
    def tone(secs, db):
        nonlocal t0
        n = int(secs * FS); t = (np.arange(n) + t0) / FS; t0 += n
        return 10 ** (db / 20) * np.sin(2 * np.pi * f * t)
    for db in range(-50, 7, 4): parts.append(tone(0.5, db))
    for hi, dur in ((-20, 0.6), (-6, 0.6), (0, 0.3), (-6, 0.05), (3, 0.6)):
        parts.append(tone(1.0, -40)); parts.append(tone(dur, hi))
    parts.append(tone(1.5, -40))
    return np.concatenate([np.zeros(PRE)] + parts).astype(np.float32)


def signals(band):
    import nulltest as NT
    cut = lambda x: np.concatenate([np.zeros((2, PRE) if x.ndim == 2 else PRE, np.float32), x[..., :3 * FS]], axis=-1)
    return [signal(BANDS[band]), cut(NT.sig_noise(-18)), cut(NT.sig_noise(-6)), cut(NT.sig_music(-10)), cut(NT.sig_music(-2))]


def residual(ref, y, seg=FS // 4):
    """mean over 0.25 s segments of each segment's residual energy re the reference's: quiet passages count as
    much as loud ones"""
    a, b = ref[:, PRE:], y[:, PRE:]
    n = a.shape[1] // seg
    e = [np.sum((a[:, i * seg:(i + 1) * seg] - b[:, i * seg:(i + 1) * seg]) ** 2) / max(np.sum(a[:, i * seg:(i + 1) * seg] ** 2), 1e-20) for i in range(n)]
    return 10 * np.log10(np.mean(e))


def fit(band, node, start, refs, xs, maxfev=260):
    amount = node / 8
    def tune(p):
        d = {f'{band}.{k}.{node}': v for k, v in zip(FREE, p)}
        d[f'{band}.makeup.{node}'] = MAKEUP[band][node]
        if PEAK is not None: d[f'{band}.peak'] = PEAK
        if FEEDBACK is not None: d[f'{band}.feedback'] = FEEDBACK
        return d
    def render(p):
        return tgp.render_many([dict(x=x, norm={**tgp.CHAR_ONLY, 'Character': amount}, raw=True, tune=tune(p)) for x in xs], 'model')
    def cost(p):
        extra = dict(zip(FREE[7:], p[7:]))
        if p[0] < 0 or p[0] > 1 or p[3] < 1 or p[4] < 0 or p[5] <= 0.01 or p[6] <= 0.1: return 1e3
        if any(v < 0 for k, v in extra.items() if k != 'slowOffset') or ('attack2' in extra and extra['attack2'] < 1): return 1e3
        return float(np.mean([10 ** (residual(r, y) / 10) for r, y in zip(refs, render(p))]))
    p0 = [start[k] for k in FREE]
    c0 = cost(p0)
    steps = dict(mix=0.1, cubic=0.2, thresh=6, ratio=0.5, knee=6, attack=max(1, start['attack'] * 0.5), release=start['release'] * 0.5,
                 gainAttack=20, gainRelease=50, attack2=100, release2=1000, slowOffset=3)
    simplex = [p0] + [[v + (steps[k] if i == j else 0) for j, (k, v) in enumerate(zip(FREE, p0))] for i in range(len(FREE))]
    r = minimize(cost, p0, method='Nelder-Mead', options=dict(maxfev=maxfev, initial_simplex=simplex, xatol=1e-3, fatol=1e-12))
    best = dict(zip(FREE, [float(v) for v in r.x]))
    return best, 10 * np.log10(c0), 10 * np.log10(r.fun)


def write_header(table):
    import re
    names = ['makeup', 'mix', 'cubic', 'thresh', 'ratio', 'knee', 'attack', 'release', 'side', 'gainAttack', 'gainRelease',
             'attack2', 'release2', 'slowOffset']
    lines = ['#pragma once', '// Generated by nulltest/fit_character.py: the character section\'s per-band settings at amount 0, 25, ..., 200 %',
             '// (order of ap::dsp::BandParam). Do not edit by hand; re-run the fit.', 'namespace ap::dsp', '{',
             f'inline constexpr int characterDetectorPeak[3] {{ {DETECTOR["low"]}, {DETECTOR["mid"]}, {DETECTOR["high"]} }};',
             'inline constexpr double characterTable[3][9][14] {']
    for b in ('low', 'mid', 'high'):
        lines.append(f'    {{ // {b}')
        for k in range(9):
            row = table[b][k]
            lines.append('        { ' + ', '.join(f'{row.get(n, 0.0):.6g}' for n in names) + ' },')
        lines.append('    },')
    lines += ['};', '}', '']
    open(HEADER, 'w').write('\n'.join(lines))


DETECTOR = {'low': 0, 'mid': 0, 'high': 1}   # chosen by fitting both (high: peak -47 dB vs mean-square -40 dB at 25 %)
PEAK = None          # detector override for experiments: 0 mean-square, 1 peak
FEEDBACK = None      # 1: feedback detector

if __name__ == '__main__':
    if '--peak' in sys.argv: PEAK = 1; sys.argv.remove('--peak')
    if '--ms' in sys.argv: PEAK = 0; sys.argv.remove('--ms')
    if '--fb' in sys.argv: FEEDBACK = 1; sys.argv.remove('--fb')
    if '--gs' in sys.argv: FREE += ['gainAttack', 'gainRelease']; sys.argv.remove('--gs')
    if '--dual' in sys.argv: FREE += ['attack2', 'release2', 'slowOffset']; sys.argv.remove('--dual')
    if '--merge' in sys.argv:           # python fit_character.py --merge low.json mid.json high.json: one table + header
        table = {}
        for path in sys.argv[sys.argv.index('--merge') + 1:]:
            for b, rows in json.load(open(path)).items():
                if all(rows): table[b] = rows
        json.dump(table, open(OUT_JSON, 'w'), indent=1)
        write_header(table)
        print('merged', list(table), '->', HEADER)
        sys.exit(0)
    only_band = sys.argv[1] if len(sys.argv) > 1 else None
    only_node = int(sys.argv[2]) if len(sys.argv) > 2 else None
    table = json.load(open(OUT_JSON)) if os.path.exists(OUT_JSON) else {}
    for band, f in BANDS.items():
        if only_band and band != only_band: continue
        xs = signals(band)
        nodes = [only_node] if only_node is not None else list(range(9))
        allrefs = tgp.render_many([dict(x=x, norm={**tgp.CHAR_ONLY, 'Character': k / 8}, raw=True) for k in nodes for x in xs], cache=True)
        refs = [allrefs[i * len(xs):(i + 1) * len(xs)] for i in range(len(nodes))]
        table.setdefault(band, [None] * 9)
        prev = None
        for k, ref in zip(nodes, refs):
            if k == 0:     # amount 0: the section is transparent (MEASUREMENTS.md 3.2)
                table[band][0] = dict(mix=0.0, cubic=0.0, thresh=0.0, ratio=1.0, knee=0.0, attack=10.0, release=100.0, makeup=0.0, side=0.1)
                prev = dict(DEFAULTS[band]); continue
            start = table[band][k] if table[band][k] else (prev or DEFAULTS[band])
            start = {**DEFAULTS[band], **{kk: vv for kk, vv in start.items() if kk in FREE}}
            if 'slowOffset' in FREE and 'slowOffset' not in (table[band][k] or {}): start['slowOffset'] = -20.0   # start with it off
            if not table[band][k]: start['makeup'] = MAKEUP[band][k]
            best, before, after = fit(band, k, start, ref, xs)
            best['side'] = 0.1; best['makeup'] = MAKEUP[band][k]
            table[band][k] = best
            prev = best
            print(f'{band} node {k} ({k * 25} %): {before:7.2f} -> {after:7.2f} dB  ', {kk: round(v, 3) for kk, v in best.items()}, flush=True)
            json.dump(table, open(OUT_JSON, 'w'), indent=1)
    if all(b in table and all(table[b]) for b in BANDS):
        write_header(table)
        print('wrote', HEADER)
