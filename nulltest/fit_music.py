"""One band's law at one amount node fitted against the reference on program material and probes together (the probe
fits of fit_bandlaw.py alone have near-equivalent minima that differ on music). The whole section is modelled in
Python (charmodel.py, identical to the plug-in to -120 dB); the other two bands are fixed, each signal weighs the same.

    python fit_music.py low 2 [start law files ...]     writes the best into measurements/character/bandlaw_music.json"""
import json, os, sys
import numpy as np
from scipy.optimize import least_squares
import tgp, nulltest as N, stress as S, ident_signals as IS, charmodel as M, fit_bandlaw as F

D = os.path.join(tgp.ROOT, 'measurements', 'character')
OUT = os.path.join(D, 'bandlaw_music.json')
KEYS = ['k', 'mix', 'thresh', 'knee', 'ratio', 'attack', 'release', 'shapeDry']
LO = [1e-3, 0.0, -60, 0.0, 1.0, 0.005, 0.5, 0.0]
HI = [10, 1.0, 0, 80, 20, 3000, 5000, 0.99]


def stereo(x):
    x = np.atleast_2d(x); return np.vstack([x, x]) if x.shape[0] == 1 else x


def signals(band):
    pad = lambda x: np.concatenate([np.zeros((2, N.PRE), np.float32), stereo(x)], -1)
    progs = [pad(N.sig_music(-10)), pad(N.sig_music(-2)), pad(N.sig_drums(-6)), pad(S.sig_bass(-6)), pad(N.sig_noise(-18))]
    probes = [stereo(IS.static_signal(band)), stereo(IS.burst_signal(band)), stereo(IS.twotone_signal(band)), stereo(IS.noise_signal(band))]
    return progs + probes


def other_laws(band, node, table, broad):
    """the fixed bands' laws at the node: band laws where fitted (between nodes interpolated), the high band from the
    broadband fit"""
    out = {}
    for b in M.NAMES:
        if b == band: continue
        if b == 'high':
            lo, hi = broad['high'][int(node)], broad['high'][min(8, int(node) + 1)]
            f = node - int(node)
            row = {k: lo[k] + (hi.get(k, lo[k]) - lo[k]) * f for k in lo if isinstance(lo[k], (int, float))}
            out[b] = dict(row, k=float(np.sqrt(6 * row['cubic'])), peak=1)
        else:
            out[b] = F.law_at(table, b, node)
    return out


if __name__ == '__main__':
    band, node = sys.argv[1], float(sys.argv[2]); node = int(node) if node == int(node) else node
    starts = sys.argv[3:] or ['bandlaw_sd.json', 'bandlaw.json', 'bandlaw_lowrich.json']
    table = json.load(open(os.path.join(D, 'bandlaw_sd.json'))); broad = json.load(open(os.path.join(D, 'fit_char.json')))
    if os.path.exists(OUT): table.update(json.load(open(OUT)))       # laws already fitted this way take precedence
    xs = signals(band)
    refs = tgp.render_many([dict(x=x.astype(np.float32), norm={**tgp.CHAR_ONLY, 'Character': node / 8}, raw=True) for x in xs], 'ref', cache=True)
    laws = other_laws(band, node, table, broad)
    bi = M.NAMES.index(band); k17 = int(round(node * 2))
    data = []
    for x, r in zip(xs, refs):
        xb, sb = M.prep(x)
        fixed = sum(M.band(xb[b], sb[b], laws[M.NAMES[b]], M.I.GAINS16[b][k17], M.WIDTH[M.NAMES[b]][k17]) for b in range(3) if b != bi)
        tgt = (r - fixed)[:, N.PRE:]
        data.append((xb[bi][:, N.PRE:], sb[bi][:, N.PRE:], tgt, np.sqrt(np.sum(r[:, N.PRE:] ** 2))))
    peak = 0 if band != 'high' else 1
    def resid(q):
        law = dict(zip(KEYS, q), peak=peak)
        return np.concatenate([((M.band(xb, sb, law, M.I.GAINS16[bi][k17], M.WIDTH[band][k17]) - t) / nrm)[:, ::4].ravel() for xb, sb, t, nrm in data])
    def score(r): return 10 * np.log10(np.mean([np.sum(((M.band(xb, sb, dict(zip(KEYS, r), peak=peak), M.I.GAINS16[bi][k17], M.WIDTH[band][k17]) - t) / nrm) ** 2) for xb, sb, t, nrm in data]))
    best = None
    for sf in starts:
        tb = json.load(open(os.path.join(D, sf)))
        st = F.law_at(tb, band, node) if f'{band}|{node}' not in tb else tb[f'{band}|{node}']
        if not st: continue
        q0 = [min(max(st.get(k, 0.0 if k == 'shapeDry' else (LO[i] + HI[i]) / 2), LO[i]), HI[i]) for i, k in enumerate(KEYS)]
        print(band, node, 'start', sf, 'mean residual %.2f dB' % score(q0), flush=True)
        r = least_squares(resid, q0, bounds=(LO, HI), x_scale='jac', max_nfev=300)
        sc = score(r.x)
        print('   ->', '%.2f dB' % sc, {k: round(v, 4) for k, v in zip(KEYS, r.x)}, flush=True)
        if best is None or sc < best[0]: best = (sc, r.x)
    res = dict(zip(KEYS, [float(v) for v in best[1]]), peak=peak, residual_dB=float(best[0]))
    out = json.load(open(OUT)) if os.path.exists(OUT) else {}
    out[f'{band}|{node}'] = res
    json.dump(out, open(OUT, 'w'), indent=1)
    print('saved', band, node, round(best[0], 2))
