"""Does the high band's detector hear a steeper low edge than its shaper? (staticn.py at 3 - 16 kHz: against 8 kHz,
ours hears 4 kHz 0.44 dB, 4.5 kHz 0.26 dB louder than the reference, and 6 - 16 kHz 0.15 dB quieter.) The character
section in Python (charmodel.py) with an extra highpass in front of the high band's detector only, normalised to 0 dB
at 8 kHz, scored against the reference on program material, character only, 100 %.

    python detedge.py [fc,Q ...]          (Q 0: first order)"""
import sys
import numpy as np
from scipy.signal import butter, sosfilt, resample_poly, iirfilter
import tgp, nulltest as N, ident as I, charmodel as CM, charsim as C, make_table as MT, track as T

FS = tgp.FS


def hp(fc, q):
    if q <= 0: return butter(1, fc, 'high', fs=FS, output='sos')
    w = 2 * np.pi * fc / FS; al = np.sin(w) / (2 * q); c = np.cos(w)
    b = np.array([(1 + c) / 2, -(1 + c), (1 + c) / 2]); a = np.array([1 + al, -2 * c, 1 - al])
    return np.concatenate([b / a[0], a / a[0]])[None, :]


def gain_at(sos, f):
    z = np.exp(-1j * 2 * np.pi * f / FS); h = 1.0
    for s in sos: h *= (s[0] + s[1] * z + s[2] * z * z) / (s[3] + s[4] * z + s[5] * z * z)
    return abs(h)


if __name__ == '__main__':
    tb, peak, _ = MT.build(0.0, [], 'bandlaw_final_v4.json', ['high'])
    row = 8; laws = {}
    for b in CM.NAMES:
        q = dict(tb[b][row]); q['k'] = float(np.sqrt(6 * q.pop('cubic'))); q['peak'] = peak[b]; laws[b] = q
    xt, fst = T.load(T.WAV); xt = resample_poly(xt, 160, 147, axis=1).astype(np.float32)
    pad = lambda x: np.concatenate([np.zeros((2, N.PRE), np.float32), (np.vstack([x, x]) if x.ndim == 1 else x).astype(np.float32)], -1)
    sigs = {'track 30': pad(xt[:, 30 * FS:36 * FS]), 'track 50': pad(xt[:, 50 * FS:56 * FS]), 'track 10': pad(xt[:, 10 * FS:16 * FS]),
            'drums -1': pad(N.sig_drums(-1)), 'pink -6': pad(N.sig_noise(-6)), 'music -10': pad(N.sig_music(-10))}
    refs = tgp.render_many([dict(x=x, norm=tgp.CHAR_ONLY, raw=True) for x in sigs.values()], 'ref', cache=True)
    pre = {k: CM.prep(x) for k, x in sigs.items()}
    fixed = {k: sum(CM.band(xb[b], sb[b], laws[CM.NAMES[b]], I.GAINS16[b][row], CM.WIDTH[CM.NAMES[b]][row]) for b in (0, 1)) for k, (xb, sb) in pre.items()}
    hl = laws['high']; p = {kk: v for kk, v in hl.items() if kk in C.NAMES}
    kk, d, gam = hl['k'], hl['shapeDry'], hl['shapeMid']
    M = 10 ** (I.GAINS16[2][row] / 20); W = 10 ** (CM.WIDTH['high'][row] / 20)

    def high(xb, sb, sos):
        s = sb[2]; m, sd = 0.5 * (s[0] + s[1]), 0.5 * (s[0] - s[1])
        if sos is not None:
            g8 = gain_at(sos, 8000.0); m = sosfilt(sos, m) / g8; sd = sosfilt(sos, sd) / g8
        g = C.band_gain(m, p, side=sd, peak=1)
        u = g * s; e = (1 - d) * (np.sin(kk * u) / kk - u); y = xb[2] - s + u + e
        mm = (0.5 * (y[0] + y[1]) + 0.5 * gam * (e[0] + e[1])) * M; ss = 0.5 * (y[0] - y[1]) * M * W
        return np.stack([mm + ss, mm - ss])

    def score(sos):
        return [N.residual_db(r, fixed[k] + high(*pre[k], sos)) for k, r in zip(sigs, refs)]
    print('                 ' + ' '.join(f'{k:>9s}' for k in sigs) + '      mean')
    s0 = score(None); print('now              ' + ' '.join(f'{v:9.2f}' for v in s0) + f' {np.mean(s0):9.2f}', flush=True)
    for a in (sys.argv[1:] or ['1500,0', '2500,0', '3500,0.5', '3000,0.707', '3500,0.707', '4000,0.707', '3000,1.0']):
        fc, q = map(float, a.split(','))
        sos = hp(fc, q)
        s = score(sos)
        print(f'hp {fc:5.0f} Q {q:4.2f}  ' + ' '.join(f'{v:9.2f}' for v in s) + f' {np.mean(s):9.2f}   (4k {20*np.log10(gain_at(sos,4000)/gain_at(sos,8000)):+.2f}, 5k {20*np.log10(gain_at(sos,5000)/gain_at(sos,8000)):+.2f} dB)', flush=True)
