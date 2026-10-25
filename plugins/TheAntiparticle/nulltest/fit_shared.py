"""Each band's law with the dynamics shared by all amount nodes: threshold, knee, ratio, attack and release fixed
(the averages of the best-fitting nodes, or given), only the shaper drive k and the mix fitted per node. The
reference's laws look like that (fit_bandlaw.py's free fits keep those five nearly constant from 100 % up and wander
only where the probes say little); a shared law also interpolates sensibly between nodes.

    python fit_shared.py low [node ...]       writes bandlaw_shared.json next to bandlaw.json"""
import json, os, sys
import numpy as np
import fit_bandlaw as F

SHARED = ['thresh', 'knee', 'ratio', 'attack', 'release']
OUT = os.path.join(os.path.dirname(F.OUT), 'bandlaw_shared.json')

if __name__ == '__main__':
    band = sys.argv[1]
    nodes = [int(a) for a in sys.argv[2:]] or list(range(1, 9))
    table = json.load(open(F.OUT))
    good = sorted((table[f'{band}|{n}']['residual_dB'], n) for n in range(1, 9) if f'{band}|{n}' in table)[:4]
    law = {k: float(np.mean([table[f'{band}|{n}'][k] for _, n in good])) for k in SHARED}
    print(band, 'shared law from nodes', [n for _, n in good], {k: round(v, 3) for k, v in law.items()}, flush=True)
    for node in nodes:
        start = table.get(f'{band}|{node}', {})
        res = F.fit(band, node, dict(start, **law), peak=bool(start.get('peak', 0)), table=table, fixed=law)
        free = table.get(f'{band}|{node}', {}).get('residual_dB')
        print(band, node, 'shared %.2f dB (free %.2f)' % (res['residual_dB'], free), {k: round(res[k], 4) for k in ('k', 'mix')}, flush=True)
        out = json.load(open(OUT)) if os.path.exists(OUT) else {}
        out[f'{band}|{node}'] = res
        json.dump(out, open(OUT, 'w'), indent=1)
