"""fit_bandlaw.py from several starting points (the band fits have local minima: the high band settles on an instant
attack from some starts, -49.7 dB, and on 1 ms from others, -56.4 dB), with both detector types; the best is saved.

    python fit_multistart.py high 1 2 3 ... [--quick]     (nodes)"""
import json, sys
import fit_bandlaw as F

STARTS = [dict(attack=1.0, release=90.0), dict(attack=0.7, release=130.0), dict(attack=300.0, release=90.0),
          dict(attack=1.0, release=2000.0)]
QUICK = [(0, dict(attack=1.0, release=90.0)), (1, dict(attack=0.7, release=130.0))]     # --quick: one start per detector

if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    band, nodes = args[0], [int(a) for a in args[1:]]
    for node in nodes:
        table = json.load(open(F.OUT))
        near = sorted((abs(int(k.split('|')[1]) - node), k) for k in table if k.startswith(band + '|'))
        base = dict(table[near[0][1]]) if near else dict(k=2.5, mix=0.72, thresh=-20, knee=44, ratio=2.2)
        best = table.get(f'{band}|{node}')
        runs = QUICK if '--quick' in sys.argv else [(pk, st) for pk in (0, 1) for st in STARTS]
        for peak, st in runs:
            r = F.fit(band, node, {**base, **st}, peak=bool(peak), table=table)
            print(band, node, 'peak', peak, 'start', st, '->', round(r['residual_dB'], 2), flush=True)
            if best is None or r['residual_dB'] < best['residual_dB'] - 0.01: best = r
        table = json.load(open(F.OUT))
        table[f'{band}|{node}'] = best
        json.dump(table, open(F.OUT, 'w'), indent=1)
        print('saved', band, node, {k: round(v, 4) for k, v in best.items()}, flush=True)
