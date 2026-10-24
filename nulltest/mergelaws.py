"""Merges band-law files for make_table.py: the low and mid bands from bandlaw_final.json, the high band's nodes from
the files given (later files win), into measurements/character/<out>.

    python mergelaws.py out.json high_laws.json [more.json ...] [--nodes=4,8]   (--nodes: from the last file only these)"""
import json, os, sys
import tgp

D = os.path.join(tgp.ROOT, 'measurements', 'character')

if __name__ == '__main__':
    out, srcs = sys.argv[1], [a for a in sys.argv[2:] if not a.startswith("--")]
    nodes = next((a[8:].split(",") for a in sys.argv[2:] if a.startswith("--nodes=")), None)
    m = {k: v for k, v in json.load(open(os.path.join(D, 'bandlaw_final.json'))).items() if not k.startswith('high')}
    for i, f in enumerate(srcs):
        for k, v in json.load(open(os.path.join(D, f))).items():
            if k.startswith('high') and (nodes is None or i < len(srcs) - 1 or k.split('|')[1] in nodes): m[k] = v
    json.dump(m, open(os.path.join(D, out), 'w'), indent=1)
    print(sorted(k for k in m if k.startswith('high')))
