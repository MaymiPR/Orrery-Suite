"""The high band's gain n(t) on program material, reference against ours: the low and mid bands' outputs (charmodel.py
with the plug-in's table, which nulls them to -58 dB and better) are taken off the reference's output; what is left
is the high band, inverted through its shaper (d u + (1 - d) sin(k u) / k) sample by sample where the band signal is
large. Prints the worst events.

    python hightrace2.py [signal] [amount 0..1]       signal from stress.SIG (default 'drums -1')"""
import sys
import numpy as np
import tgp, nulltest as N, stress as S, ident as I, charmodel as CM, make_table as MT


def table_laws(row):
    t, peak, _ = MT.build(0.0, [], 'bandlaw_final_v2.json', ['high'])
    out = {}
    for b in CM.NAMES:
        r = dict(t[b][row]); r['k'] = float(np.sqrt(6 * r.pop('cubic'))); r['peak'] = peak[b]
        out[b] = r
    return out


def invert(yb, k, d):
    """u from F(u) = d u + (1 - d) sin(k u) / k on its rising branch (Newton from u = yb)"""
    u = yb.copy()
    lim = np.arccos(np.clip(-d / (1 - d), -1, 1)) / k * 0.999
    for _ in range(30):
        f = d * u + (1 - d) * np.sin(k * u) / k - yb; fp = d + (1 - d) * np.cos(k * u)
        u = np.clip(u - f / np.maximum(fp, 1e-3), -lim, lim)
    return u


if __name__ == '__main__':
    sig = sys.argv[1] if len(sys.argv) > 1 else 'drums -1'
    amt = float(sys.argv[2]) if len(sys.argv) > 2 else 0.5
    row = int(round(amt * 16))
    x = np.atleast_2d(S.SIG[sig]()); x = np.vstack([x, x]) if x.shape[0] == 1 else x
    x = np.concatenate([np.zeros((2, N.PRE), np.float32), x], -1)
    j = dict(x=x, norm={**tgp.CHAR_ONLY, 'Character': amt}, raw=True)
    r = tgp.render_many([j], 'ref', cache=True)[0]; o = tgp.render_many([j], 'model')[0]
    laws = table_laws(row)
    xb, sb = CM.prep(x)
    ys = [CM.band(xb[b], sb[b], laws[CM.NAMES[b]], I.GAINS16[b][row], CM.WIDTH[CM.NAMES[b]][row]) for b in range(3)]
    print('charmodel vs plug-in: %.1f dB' % N.residual_db(o, sum(ys)), '  ref vs plug-in: %.1f dB' % N.residual_db(r, o))
    M = 10 ** (I.GAINS16[2][row] / 20); k, d = laws['high']['k'], laws['high']['shapeDry']
    s = sb[2][0]
    res = {}
    for who, yh in (('ref', (r - ys[0] - ys[1])[0]), ('ours', ys[2][0])):
        u = invert(yh / M - (xb[2][0] - s), k, d)
        with np.errstate(all='ignore'): res[who] = np.where(np.abs(s) > 0.05, u / s, np.nan)
    e = (r - o)[0]
    W = 480
    en = np.array([np.sum(e[a:a + W] ** 2) for a in range(N.PRE, len(e) - W, W)])
    for jw in np.argsort(en)[::-1][:4]:
        a = N.PRE + jw * W; c = a + int(np.argmax(np.abs(e[a:a + W])))
        print(f'event at {c} ({c / tgp.FS:.3f} s):')
        for q in range(c - 240, c + 241, 24):
            seg = slice(q, q + 24)
            nr, no = np.nanmedian(res['ref'][seg]), np.nanmedian(res['ours'][seg])
            print(f'   {q - c:+5d}  |s| max {np.max(np.abs(s[seg])):.3f}  n ref {20 * np.log10(nr):+7.3f}  ours {20 * np.log10(no):+7.3f}  '
                  f'diff {20 * np.log10(no / nr):+.3f} dB   err rms {np.sqrt(np.mean(e[seg] ** 2)):.4f}')
