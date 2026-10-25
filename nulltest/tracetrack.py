"""The high band's gain g(t) on the demo track, reference against ours, read sample by sample: the low and mid bands
(charmodel.py with the current laws) are taken off the reference's mid output, and what is left is solved for the
gain through the measured shaper with its mid/side difference (u + (1 + gamma)(1 - d)(sin(k u) / k - u) per channel,
u = g s). Ours is the gain computer's own output. Prints how the difference depends on what the detector is doing
(attacking, holding, releasing) and on the level, and saves both traces.

    python tracetrack.py [start s] [secs] [amount 0..1] [laws]"""
import os, sys
import numpy as np
from scipy.signal import resample_poly
import tgp, nulltest as N, ident as I, charmodel as CM, charsim as C, make_table as MT, track as T

if __name__ == '__main__':
    t0 = float(sys.argv[1]) if len(sys.argv) > 1 else 30
    secs = float(sys.argv[2]) if len(sys.argv) > 2 else 6
    amt = float(sys.argv[3]) if len(sys.argv) > 3 else 0.5
    lawfile = sys.argv[4] if len(sys.argv) > 4 else 'bandlaw_final_v4.json'
    row = int(round(amt * 16)); fs = tgp.FS
    xt, fst = T.load(T.WAV)
    xt = resample_poly(xt, 160, 147, axis=1).astype(np.float32) if fst == 44100 else xt
    x = np.concatenate([np.zeros((2, N.PRE), np.float32), xt[:, int(t0 * fs):int((t0 + secs) * fs)]], -1)
    j = dict(x=x, norm={**tgp.CHAR_ONLY, 'Character': amt}, raw=True)
    r = tgp.render_many([j], 'ref', cache=True)[0].astype(np.float64)
    o = tgp.render_many([j], 'model')[0].astype(np.float64)
    tb, peak, _ = MT.build(0.0, [], lawfile, ['high'])
    laws = {}
    for b in CM.NAMES:
        q = dict(tb[b][row]); q['k'] = float(np.sqrt(6 * q.pop('cubic'))); q['peak'] = peak[b]; laws[b] = q
    xb, sb = CM.prep(x)
    ys = [CM.band(xb[b], sb[b], laws[CM.NAMES[b]], I.GAINS16[b][row], CM.WIDTH[CM.NAMES[b]][row]) for b in range(3)]
    print('charmodel vs plug-in %.1f dB, ref vs plug-in %.1f dB' % (N.residual_db(o, sum(ys)), N.residual_db(r, o)))
    hl = laws['high']; k, d, gam = hl['k'], hl.get('shapeDry', 0.0), hl.get('shapeMid', 0.0)
    M = 10 ** (I.GAINS16[2][row] / 20)
    s = sb[2]; lin = (xb[2] - s)
    p = {kk: v for kk, v in hl.items() if kk in C.NAMES}
    g_ours = C.band_gain(0.5 * (s[0] + s[1]), p, side=0.5 * (s[0] - s[1]), peak=int(hl['peak']))
    def mid_out(g):
        u = g * s; e = (1 - d) * (np.sin(k * u) / k - u)
        return 0.5 * (lin[0] + lin[1] + u[0] + u[1]) + 0.5 * (1 + gam) * (e[0] + e[1])
    target = 0.5 * ((r - ys[0] - ys[1])[0] + (r - ys[0] - ys[1])[1]) / M
    chk = 0.5 * (ys[2][0] + ys[2][1]) / M
    g = g_ours.copy(); gc = g_ours.copy()
    for _ in range(25):                                     # Newton per sample on g (derivative numerically)
        for tgt, gg in ((target, g), (chk, gc)):
            f = mid_out(gg) - tgt; fp = (mid_out(gg + 1e-5) - mid_out(gg - 1e-5)) / 2e-5
            gg -= np.where(np.abs(fp) > 1e-4, f / np.where(np.abs(fp) > 1e-4, fp, 1), 0)
            np.clip(gg, 0.05, 1.5, out=gg)
    sm = np.abs(0.5 * (s[0] + s[1]))
    ok = (sm > 0.02 * np.max(sm)) & (np.arange(len(sm)) > N.PRE)
    print('inversion check on ours: median |error| %.5f dB' % np.median(np.abs(20 * np.log10(gc[ok] / g_ours[ok]))))
    dd = 20 * np.log10(g_ours / g)                          # ours / ref, dB
    # what the detector is doing: our gain's slope over 1 ms
    W = fs // 1000
    slope = np.zeros_like(g_ours); slope[W:] = 20 * np.log10(g_ours[W:] / g_ours[:-W])
    gr = -20 * np.log10(g_ours)
    print(f'samples used {ok.sum()} of {len(ok) - N.PRE}; ours/ref overall median {np.median(dd[ok]):+.3f} dB, '
          f'weighted by s^2 {np.average(dd[ok], weights=sm[ok] ** 2):+.3f} dB')
    for name, sel in (('attacking (gain falling > 0.05 dB/ms)', slope < -0.05), ('steady', np.abs(slope) <= 0.05),
                      ('releasing (rising > 0.05 dB/ms)', slope > 0.05)):
        q = ok & sel
        if q.sum() > 100: print(f'  {name:40s} share {100 * q.sum() / ok.sum():4.1f} %  ours/ref median {np.median(dd[q]):+.3f} dB  p10/p90 {np.percentile(dd[q], 10):+.3f}/{np.percentile(dd[q], 90):+.3f}')
    for lo in (0, 1, 2, 3, 4, 6, 8):
        q = ok & (gr >= lo) & (gr < (lo + 1 if lo < 4 else lo + 2))
        if q.sum() > 100: print(f'  our reduction {lo}..{lo + (1 if lo < 4 else 2)} dB: share {100 * q.sum() / ok.sum():4.1f} %  ours/ref median {np.median(dd[q]):+.3f} dB')
    np.save(os.path.join(tgp.ROOT, 'measurements', 'character', f'tracetrack_{t0:g}_{amt}.npy'), np.stack([g, g_ours, sm, ok.astype(float)]))
