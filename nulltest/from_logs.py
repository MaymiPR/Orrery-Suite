"""Puts the results printed in fit logs (measurements/character/<prefix>_<band>_<node>.log, the last result line of
each) into bandlaw.json, for runs whose own saves may have overwritten each other.

    python from_logs.py nl"""
import ast, glob, json, os, re, sys
import fit_bandlaw as F

if __name__ == '__main__':
    prefix = sys.argv[1]
    table = json.load(open(F.OUT))
    for path in sorted(glob.glob(os.path.join(os.path.dirname(F.OUT), f'{prefix}_*.log'))):
        lines = [l for l in open(path) if re.match(r'^(low|mid|high) \d ', l)]
        if not lines: continue
        band, node, rest = lines[-1].split(' ', 2)
        table[f'{band}|{node}'] = ast.literal_eval(rest.strip())
        print(band, node, round(table[f'{band}|{node}']['residual_dB'], 2))
    json.dump(table, open(F.OUT, 'w'), indent=1)
