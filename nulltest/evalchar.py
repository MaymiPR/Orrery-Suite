"""Quick evaluation of character-section changes: the stress groups at 48 kHz, block 512 that exercise the character
section (character-only amounts, levels, combos), printed against the last saved stress results; nothing is written.

    python evalchar.py"""
import json, os
import numpy as np
import stress as S

if __name__ == '__main__':
    S.RUNS = S.RUNS[:1]
    old = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'results_stress.json')))
    res = {}
    for g in ('amount char-only', 'levels', 'combos'):
        res.update(S.run(g))
    d = [res[k] - old[k] for k in res if k in old]
    print(f'{len(res)} cases: median {np.median(list(res.values())):.2f} dB (saved {np.median([old[k] for k in res if k in old]):.2f}), '
          f'mean change {np.mean(d):+.2f} dB, worst change {max(d):+.2f} dB, best {min(d):+.2f} dB')
