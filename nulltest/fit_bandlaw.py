"""Fits one character band at one amount node to the reference, band output against band output.

The crossover is exact (BandFilters.h), so the band's input x_b is known. Its output in the reference is the total output
minus the other two bands. Those get the probe tone well down (the mid band still passes a 70 Hz tone at -21 dB, enough
to compress it), so each is modelled by its own fitted law when bandlaw.json has one, else treated as linear. The model is
the plug-in's own gain computer (charsim.dll, BandDynamics.h) followed by the sine shaper, both on what the band's
nonlinear stage hears (s_b, ident.nl_bands: the band with steeper edges); the rest of the band passes linearly:
    y_b = M (x_b - s_b + sin(k n(t) s_b) / k),   n from s_b
Free: the shaper drive k, the mix, the static law (threshold, knee, ratio), the detector times, and optionally the
slow second detector. Robust least squares on the probe signals of ident_signals.py, with soft-L1 loss.

    python fit_bandlaw.py mid 4 [--dual] [--peak] [--force] [--from <node>] [--rich] [--out file.json]
Writes measurements/character/bandlaw.json (one entry per band / node), read by fit_character.py --from-bandlaw."""
import json, os, sys
import numpy as np
from scipy.optimize import least_squares
import tgp, ident as I, ident_signals as S, charsim as C

FS = tgp.FS
OUT = os.path.join(tgp.ROOT, 'measurements', 'character', 'bandlaw.json')
if '--out' in sys.argv: OUT = os.path.join(os.path.dirname(OUT), sys.argv[sys.argv.index('--out') + 1])
BI = {'low': 0, 'mid': 1, 'high': 2}
RICH = '--rich' in sys.argv          # also a two-tone and a band-noise staircase (they separate peak from mean-square detectors)


def band_out(xb, law, sb=None):
    """one band's output (before make-up) under a fitted law; sb: what its nonlinear stage hears (ident.nl_bands),
    None: the band signal itself"""
    sb = xb if sb is None else sb
    p = {k: v for k, v in law.items() if k not in ('k', 'residual_dB', 'median_abs_err', 'peak')}
    g = C.band_gain(sb, p, peak=bool(law.get('peak', 0)))
    return xb - sb + np.sin(law['k'] * g * sb) / law['k']


def law_at(table, band, node):
    """a band's law at a node, between nodes the mean of the neighbours (as the plug-in interpolates its table)"""
    if f'{band}|{node}' in table or node == int(node): return table.get(f'{band}|{int(node)}')
    lo, hi = table.get(f'{band}|{int(node)}'), table.get(f'{band}|{int(node) + 1}')
    if not lo or not hi: return None
    f = node - int(node)
    out = {k: lo[k] + (hi.get(k, lo[k]) - lo[k]) * f for k in lo if isinstance(lo[k], (int, float)) and k != 'peak'}
    out['k'] = float(np.sqrt(lo['k'] ** 2 + (hi['k'] ** 2 - lo['k'] ** 2) * f))      # the table interpolates k^2 / 6
    out['peak'] = lo.get('peak', 0)
    return out


def load(band, node, table=None):
    bi = BI[band]
    table = table or {}
    names = {v: k for k, v in BI.items()}
    xs = [S.static_signal(band), S.burst_signal(band)]
    if RICH: xs += [S.twotone_signal(band), S.noise_signal(band)]
    ys = tgp.render_many([dict(x=x, norm={**tgp.CHAR_ONLY, 'Character': node / 8}, raw=True) for x in xs], cache=True)
    M = [10 ** (I.gain_db(b, node) / 20) for b in range(3)]
    data = []
    for x, y in zip(xs, ys):
        xb, sb = I.bands(x)[:, 0], I.nl_bands(x)[:, 0]
        other = 0.0
        for b in range(3):
            if b == bi: continue
            law = law_at(table, names[b], node)
            other = other + M[b] * (band_out(xb[b], law, sb[b]) if law else xb[b])
        data.append((xb[bi], sb[bi], (y[0] - other) / M[bi]))
    return data


def fit(band, node, start, dual=False, peak=False, decim=3, table=None, fixed=None):
    """fixed: parameters held at given values (the band's shared law); the rest are fitted"""
    data = load(band, node, table)
    fixed = fixed or {}
    keys = ['k', 'mix', 'thresh', 'knee', 'ratio', 'attack', 'release'] + (['attack2', 'release2', 'slowOffset'] if dual else [])
    lo = [1e-3, 0.0, -60, 0.0, 1.0, 0.005, 0.5] + ([1, 1, -60] if dual else [])
    hi = [10, 2.0, 0, 80, 20, 3000, 5000] + ([20000, 20000, 20] if dual else [])
    sel = [i for i, k_ in enumerate(keys) if k_ not in fixed]
    keys, lo, hi = [keys[i] for i in sel], [lo[i] for i in sel], [hi[i] for i in sel]
    def resid(q):
        p = dict(zip(keys, q)); p.update(fixed); kk = p.pop('k')
        out = []
        for xb, sb, yb in data:
            g = C.band_gain(sb, p, peak=peak)
            out.append((xb - sb + np.sin(kk * g * sb) / kk - yb)[::decim])
        return np.concatenate(out)
    p0 = [min(max(start.get(k_, (lo[i] + hi[i]) / 2), lo[i]), hi[i]) for i, k_ in enumerate(keys)]
    r = least_squares(resid, p0, bounds=(lo, hi), loss='soft_l1', f_scale=1e-4, x_scale='jac', max_nfev=400)
    e = resid(r.x)
    ref_rms = np.sqrt(np.mean(np.concatenate([yb[::decim] for _, _, yb in data]) ** 2))
    res = dict(zip(keys, [float(v) for v in r.x])); res.update(fixed)
    res['residual_dB'] = float(20 * np.log10(np.sqrt(np.mean(e ** 2)) / ref_rms))
    res['median_abs_err'] = float(np.median(np.abs(e)))
    res['peak'] = int(peak)
    return res


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')][:2]
    band, node = args[0], float(args[1])
    node = int(node) if node == int(node) else node
    table = json.load(open(OUT)) if os.path.exists(OUT) else {}
    start = table.get(f'{band}|{node}', {})
    if '--from' in sys.argv: start = table[f'{band}|{sys.argv[sys.argv.index("--from") + 1]}']   # start from another node
    if '--from-broad' in sys.argv:          # start from the broadband fit (fit_char.json)
        row = json.load(open(os.path.join(os.path.dirname(OUT), 'fit_char.json')))[band][node]
        start = dict(row, k=float(np.sqrt(6 * row['cubic'])))
    near = sorted((abs(int(k.split('|')[1]) - node), k) for k in table if k.startswith(band + '|'))
    if not start and near: start = table[near[0][1]]       # the nearest fitted node of the same band
    if not start:   # defaults from the mid band at 100 % (the first one identified)
        start = dict(k=2.5, mix=0.72, thresh=-20, knee=44, ratio=2.2, attack=300, release=90, attack2=500, release2=2000, slowOffset=-20)
    res = fit(band, node, start, dual='--dual' in sys.argv, peak='--peak' in sys.argv, table=table)
    print(band, node, {k: round(v, 4) for k, v in res.items()}, flush=True)
    table = json.load(open(OUT))         # again: fits of other bands / nodes may have finished meanwhile
    old = table.get(f'{band}|{node}')
    if '--force' in sys.argv: old = None
    if old is None or res['residual_dB'] < old.get('residual_dB', 0) - 0.01:
        table[f'{band}|{node}'] = res
        json.dump(table, open(OUT, 'w'), indent=1)
        print('saved')
