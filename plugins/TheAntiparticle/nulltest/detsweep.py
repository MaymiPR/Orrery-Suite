"""The high band's steady-state gain reduction against pump frequency and level (detspec.py's probe method), reference
and ours, saved to measurements/character/detsweep_<amount>.json:
  - at high frequencies (inside the band) the curve against level is the static law the detector applies to a sine;
  - near the crossover the level shift that maps a curve onto the in-band one is what the detector's filter passes.

    python detsweep.py [amount 0..1]"""
import json, os, sys
import numpy as np
import tgp
from hightrace import FP, PROBE, gain_trace

FS = tgp.FS; PRE = FS
FREQS = [2000, 2400, 2700, 3000, 3500, 4000, 6000, 9000, 13000]
LEVELS = list(range(-27, 1, 3))

if __name__ == '__main__':
    amt = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5
    n = PRE + 2 * FS; t = np.arange(n) / FS
    probe = PROBE * np.sin(2 * np.pi * FP * t)
    combos = [(f, l) for f in FREQS for l in LEVELS] + [(0, -200)]
    jobs = [dict(x=(np.sin(2 * np.pi * f * t) * 10 ** (l / 20) + s * probe).astype(np.float32),
                 norm={**tgp.CHAR_ONLY, 'Character': amt}) for f, l in combos for s in (1, -1)]
    rr = tgp.render_many(jobs, 'ref', cache=True); oo = tgp.render_many(jobs, 'model')
    seg = slice(PRE + FS // 2, n - FS // 4)
    g = lambda y, i: np.median(gain_trace(0.5 * (y[2 * i][0] - y[2 * i + 1][0]), n)[seg])
    br, bo = g(rr, len(combos) - 1), g(oo, len(combos) - 1)
    out = {'freqs': FREQS, 'levels': LEVELS, 'ref': [], 'ours': []}
    for i, (f, l) in enumerate(combos[:-1]):
        out['ref'].append(20 * np.log10(g(rr, i) / br)); out['ours'].append(20 * np.log10(g(oo, i) / bo))
    for k in ('ref', 'ours'): out[k] = np.array(out[k]).reshape(len(FREQS), len(LEVELS)).round(4).tolist()
    json.dump(out, open(os.path.join(tgp.ROOT, 'measurements', 'character', f'detsweep_{amt}.json'), 'w'))
    print('level  ' + ' '.join(f'{f:>13d}' for f in FREQS))
    for j, l in enumerate(LEVELS):
        print(f'{l:5d}  ' + ' '.join(f'{out["ref"][i][j]:+6.2f}/{out["ours"][i][j]:+6.2f}' for i in range(len(FREQS))))
