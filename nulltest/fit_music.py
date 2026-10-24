"""One band's law at one amount node fitted against the reference on program material and probes together (the probe
fits of fit_bandlaw.py alone have near-equivalent minima that differ on music). The whole section is modelled in
Python (charmodel.py, identical to the plug-in to -120 dB); the other two bands are fixed, each signal weighs the same.

    python fit_music.py low 2 [start law files ... | broad] [--truepeak] [--shaper]     writes the best into
    measurements/character/bandlaw_music.json (bandlaw_music_tp.json with --truepeak)
--loud: the loud program signals too (drums -1, pink -6, music 0, bright -8), where the high band works hardest;
writes bandlaw_music[_tp]_loud[_shp].json
--static W: (high band) the reference's steady gain curve too (staticn.py: tone, two tones, band noise at -18 ... 0 dBFS,
read through the measured shaper), its error in nepers weighed by W; writes ..._static.json
--shaper: the shaper's k and dry share fixed at the values read directly with anti-phase tones (shapeside.py,
measurements/character/shaper_<band>.txt); the other bands from bandlaw_final.json; writes bandlaw_music_shp.json"""
import json, os, sys
import numpy as np
from scipy.optimize import least_squares
import tgp, charsim as C, ident as I, nulltest as N, stress as S, ident_signals as IS, charmodel as M, fit_bandlaw as F

D = os.path.join(tgp.ROOT, 'measurements', 'character')
OUT = os.path.join(D, ('bandlaw_music_tp' if '--truepeak' in sys.argv else 'bandlaw_music') + ('_rel' if '--relative' in sys.argv else '')
                   + ('_loud' if '--loud' in sys.argv else '') + ('_shp' if '--shaper' in sys.argv else '')
                   + ('_static' if '--static' in sys.argv else '') + '.json')
SHAPER = '--shaper' in sys.argv


def measured_shaper(band, node):
    """k and the dry share at a node of the 25 % grid from shapeside.py's readings (lines 'SHAPER band amount k d ...')"""
    rows = [l.split() for l in open(os.path.join(D, f'shaper_{band}.txt')) if l.startswith('SHAPER')]
    for r in rows:
        if abs(float(r[2]) * 8 - node) < 1e-6: return float(r[3]), float(r[4])
    raise KeyError(node)


def measured_shape_mid(band, node):
    p = os.path.join(D, f'shapemid_{band}.txt')
    if not os.path.exists(p): return 0.0
    return {int(l.split()[1]): float(l.split()[2]) - 1 for l in open(p) if l.startswith('SHAPEMID')}.get(int(round(node * 2)), 0.0)
KEYS = ['k', 'mix', 'thresh', 'knee', 'ratio', 'attack', 'release', 'shapeDry']
LO = [1e-3, 0.0, -60, 0.0, 1.0, 0.005, 0.5, 0.0]
HI = [10, 1.0, 0, 80, 20, 3000, 5000, 0.99]


def stereo(x):
    x = np.atleast_2d(x); return np.vstack([x, x]) if x.shape[0] == 1 else x


def signals(band):
    pad = lambda x: np.concatenate([np.zeros((2, N.PRE), np.float32), stereo(x)], -1)
    progs = [pad(N.sig_music(-10)), pad(N.sig_music(-2)), pad(N.sig_drums(-6)), pad(S.sig_bass(-6)), pad(N.sig_noise(-18))]
    if '--loud' in sys.argv: progs += [pad(N.sig_drums(-1)), pad(N.sig_noise(-6)), pad(N.sig_music(0)), pad(S.sig_bright(-8))]
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
            if SHAPER:
                kk, dd = measured_shaper(b, node); out[b].update(k=kk, shapeDry=dd, shapeMid=measured_shape_mid(b, node))
        else:
            out[b] = F.law_at(table, b, node)
    return out


if __name__ == '__main__':
    band, node = sys.argv[1], float(sys.argv[2]); node = int(node) if node == int(node) else node
    skip = {sys.argv.index('--static') + 1} if '--static' in sys.argv else set()
    starts = [a for i, a in enumerate(sys.argv) if i >= 3 and i not in skip and not a.startswith('--')] or ['bandlaw_sd.json', 'bandlaw.json', 'bandlaw_lowrich.json']
    table = json.load(open(os.path.join(D, 'bandlaw_final.json' if SHAPER else 'bandlaw_sd.json'))); broad = json.load(open(os.path.join(D, 'fit_char.json')))
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
    RELATIVE = '--relative' in sys.argv       # objective: mean of the per-signal residuals in dB (no signal traded away)
    peak = 0 if band != 'high' else (2 if '--truepeak' in sys.argv else 1)
    if band == 'high':        # the high band also has a slow second detector (the broadband fit's structure)
        KEYS = KEYS + ['attack2', 'release2', 'slowOffset']; LO = LO + [1.0, 1.0, -60.0]; HI = HI + [20000.0, 20000.0, 20.0]
    fixed_law = {}
    if SHAPER:              # k and shapeDry measured, not fitted
        kk, dd = measured_shaper(band, node); fixed_law = dict(k=kk, shapeDry=dd, shapeMid=measured_shape_mid(band, node))
        drop = [i for i, k in enumerate(KEYS) if k in fixed_law]
        KEYS = [k for i, k in enumerate(KEYS) if i not in drop]; LO = [v for i, v in enumerate(LO) if i not in drop]; HI = [v for i, v in enumerate(HI) if i not in drop]
        print('shaper fixed', fixed_law, flush=True)
    static_w = float(sys.argv[sys.argv.index('--static') + 1]) if '--static' in sys.argv else 0.0
    static_data = None
    if static_w > 0 and band == 'high':
        import staticn as SN
        ref_n = np.load(os.path.join(D, f'staticn_{node / 8}_8000.npy'))[:, 0]
        sel = [i for i, l in enumerate(SN.LEVELS) if -18 <= l <= 0]
        static_data = []
        for kk, kind in enumerate(('tone', 'twotone', 'noise')):
            xs_, ns_ = SN.staircase(kind, 8000.0)
            xs_ = xs_[:I.PRE + (max(sel) + 1) * ns_]
            static_data.append((M.I.nl_bands(xs_)[2, 0], ns_, ref_n[kk, sel], sel))
    def static_resid(law):
        if static_data is None: return np.zeros(0)
        pp = {k: v for k, v in law.items() if k in C.NAMES}
        out = []
        for sb, ns_, rn, sel in static_data:
            gg = C.band_gain(sb, pp, peak=int(law.get('peak', 0)))
            nn = np.array([np.median(gg[I.PRE + i * ns_ + ns_ * 2 // 3:I.PRE + (i + 1) * ns_]) for i in sel])
            out.append(np.log(nn) - rn / 8.685889638)
        return static_w * np.concatenate(out)
    wts = np.ones(len(data))                 # --relative: each signal weighed by its starting residual (set per start)
    def per_signal(q):
        law = dict(zip(KEYS, q), peak=peak, **fixed_law)
        return [np.sum(((M.band(xb, sb, law, M.I.GAINS16[bi][k17], M.WIDTH[band][k17]) - t) / nrm) ** 2) for xb, sb, t, nrm in data]
    def resid(q):
        law = dict(zip(KEYS, q), peak=peak, **fixed_law)
        return np.concatenate([w * ((M.band(xb, sb, law, M.I.GAINS16[bi][k17], M.WIDTH[band][k17]) - t) / nrm)[:, ::4].ravel()
                               for w, (xb, sb, t, nrm) in zip(wts, data)] + [static_resid(law)])
    def score(r):
        e = per_signal(r)
        return float(np.mean(10 * np.log10(e))) if RELATIVE else 10 * np.log10(np.mean(e))
    best = None
    for sf in starts:
        if sf == 'broad':         # the broadband fit (fit_char.json)
            row = broad[band][int(node)]
            st = dict(row, k=float(np.sqrt(6 * row['cubic'])))
        else:
            tb = json.load(open(os.path.join(D, sf)))
            st = F.law_at(tb, band, node) if f'{band}|{node}' not in tb else tb[f'{band}|{node}']
        if not st: continue
        q0 = [min(max(st.get(k, 0.0 if k == 'shapeDry' else (LO[i] + HI[i]) / 2), LO[i]), HI[i]) for i, k in enumerate(KEYS)]
        if RELATIVE: wts[:] = 1 / np.sqrt(np.array(per_signal(q0)))
        print(band, node, 'start', sf, 'mean residual %.2f dB' % score(q0), 'per signal', ' '.join('%.1f' % (10 * np.log10(v)) for v in per_signal(q0)), flush=True)
        r = least_squares(resid, q0, bounds=(LO, HI), x_scale='jac', max_nfev=300)
        sc = score(r.x)
        print('   per signal', ' '.join('%.1f' % (10 * np.log10(v)) for v in per_signal(r.x)), flush=True)
        if static_data is not None:
            print('   static curve rms %.3f dB' % (8.686 * np.sqrt(np.mean((static_resid(dict(zip(KEYS, r.x), peak=peak, **fixed_law)) / static_w) ** 2))), flush=True)
        print('   ->', '%.2f dB' % sc, {k: round(v, 4) for k, v in zip(KEYS, r.x)}, flush=True)
        if best is None or sc < best[0]: best = (sc, r.x)
    res = dict(zip(KEYS, [float(v) for v in best[1]]), peak=peak, residual_dB=float(best[0]), **fixed_law)
    out = json.load(open(OUT)) if os.path.exists(OUT) else {}
    out[f'{band}|{node}'] = res
    json.dump(out, open(OUT, 'w'), indent=1)
    print('saved', band, node, round(best[0], 2))
