"""Gain-reduction meters on per-band tone staircases (reference/stairs_bands.f32, RefCapture films of both plug-ins):
per step the band's gain reduction (our model, charmodel with the plug-in's table, default 100 %), the bars and the
target fill of the reference and of ours.

    python stairgr.py"""
import json, os
import numpy as np
import tgp, charmodel as CM, charsim as C, ident as I
from hightrace2 import table_laws

D = os.path.join(tgp.ROOT, 'measurements', 'meters')
FS = tgp.FS


def band_gr(x, band, row=8):
    laws = table_laws(row); b = CM.NAMES.index(band)
    xb, sb = CM.prep(np.stack([x, x]))
    law = laws[band]; p = {k: v for k, v in law.items() if k not in ('k', 'residual_dB', 'median_abs_err', 'peak', 'shapeMid')}
    g, gr = C.band_gain(0.5 * (sb[b][0] + sb[b][1]), p, side=0.5 * (sb[b][0] - sb[b][1]), peak=int(law.get('peak', 0)), want_gr=True)
    return gr


if __name__ == '__main__':
    x = np.fromfile(os.path.join(tgp.ROOT, 'reference', 'stairs_bands.f32'), np.float32).astype(float)
    steps = json.load(open(os.path.join(tgp.ROOT, 'reference', 'stairs_bands.json')))
    films = {w: np.load(os.path.join(D, f'grtarget_stairs_{w}.npz')) for w in ('ref', 'ours')}
    grs = {b: band_gr(x, b) for b in CM.NAMES}
    rows = []
    for s in steps:
        b = ('high', 'mid', 'low').index(s['band'])          # the films' meter order
        a, e = int((s['start'] + 0.6) * FS), int(s['end'] * FS)
        gr = float(np.median(grs[s['band']][a:e]))
        r = dict(band=s['band'], level=s['level'], gr=gr)
        for w, f in films.items():
            sel = (f['t'] > s['start'] + 0.6) & (f['t'] < s['end'] - 0.05)
            r[w + '_bars'] = int(np.median(f['bars'][sel, b])); r[w + '_fill'] = int(np.median(f['fill'][sel, b]))
        rows.append(r)
        print(f"{s['band']:5s} {s['level']:6.1f} dBFS  GR {gr:5.2f} dB   ref {r['ref_bars']:2d} bars fill {r['ref_fill']:3d}   ours {r['ours_bars']:2d} bars fill {r['ours_fill']:3d}")
    json.dump(rows, open(os.path.join(D, 'stairgr.json'), 'w'), indent=1)
