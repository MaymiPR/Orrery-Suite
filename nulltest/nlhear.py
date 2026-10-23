"""What each character band's nonlinear stage (detector and shaper) hears outside its band.

A quiet probe tone in the band (-30 dBFS) under a loud interferer stepped across frequency. The interferer changes
the probe's gain through the band's detector and through cross-modulation in its shaper. For each step, the
interferer level at the nonlinear stage's input that reproduces the reference's probe gain is solved for (the band's
fitted law, bandlaw.json; the other bands run through their own laws, since the interferer may compress them and
they pass a little of the probe), and compared with how much of it the band's FIR filter passes. Below the FIR value: the
nonlinear stage hears a steeper band than the audio passes.

    python nlhear.py mid [loud dBFS] [node]"""
import json, sys
import numpy as np
from scipy.optimize import brentq
import tgp, ident as I, ident_signals as S, fit_bandlaw as F, charsim as C

FS = tgp.FS
INTERFERERS = {'low': [130, 160, 200, 250, 300, 400, 500, 700, 1000, 1500, 2000, 3000, 5000, 8000, 12000],
               'mid': [30, 50, 70, 100, 130, 160, 200, 1600, 2000, 2700, 3500, 5000, 6500, 8000, 10000, 12000, 16000],
               'high': [200, 400, 700, 1000, 1300, 1600, 2000, 2400, 2700, 3200]}
PROBE = {'low': 60.0, 'mid': 700.0, 'high': 9000.0}
SEG = 2.0


def fir_mag(b, f):
    h = I.IRS[b]
    return abs(np.sum(h * np.exp(-2j * np.pi * f / FS * np.arange(len(h)))))


def run(band, loud_db=-2.0, node=4):
    bi = F.BI[band]; freqs = INTERFERERS[band]; fp = PROBE[band]
    table = json.load(open(F.OUT))
    n = S.PRE + int(len(freqs) * SEG * FS); t = np.arange(n) / FS
    loud = np.zeros(n)
    for i, f in enumerate(freqs):
        a = S.PRE + int(i * SEG * FS); loud[a:a + int(SEG * FS)] = 10 ** (loud_db / 20) * np.sin(2 * np.pi * f * t[a:a + int(SEG * FS)])
    probe = 10 ** (-30 / 20) * np.sin(2 * np.pi * fp * t); probe[:S.PRE] = 0
    x = (loud + probe).astype(np.float32)
    y = tgp.render_many([dict(x=x, norm={**tgp.CHAR_ONLY, 'Character': node / 8}, raw=True)], cache=True)[0][0]
    pb = I.bands(probe)[:, 0]
    xb, sb = I.bands(x)[:, 0], I.nl_bands(x)[:, 0]
    names = ['low', 'mid', 'high']
    law = table[f'{band}|{node}']; p = {k: v for k, v in law.items() if k not in ('k', 'residual_dB', 'median_abs_err', 'peak')}
    Mo = [10 ** (I.GAINS[b][node] / 20) for b in range(3)]; k = law['k']
    out = []
    for i, f in enumerate(freqs):
        a = S.PRE + int(i * SEG * FS); s = slice(a + FS, a + int(SEG * FS)); w = slice(a - FS // 2, a + int(SEG * FS))
        ss = slice(FS + FS // 2, None)
        e = np.exp(-2j * np.pi * fp * t[w][ss]); pd = abs(np.dot(probe[w][ss], e))
        ref = abs(np.dot(y[w][ss], e)) / pd
        tone = 10 ** (loud_db / 20) * np.sin(2 * np.pi * f * t[w])
        others = 0.0      # the other bands, each through its own law (the interferer may compress them)
        for b in range(3):
            if b == bi: continue
            lw = table.get(f'{names[b]}|{node}')
            others = others + Mo[b] * (F.band_out(xb[b][w], lw, sb[b][w]) if lw else xb[b][w])
        def gain(att):
            s_in = pb[bi][w] + att * tone
            g = C.band_gain(s_in, p, peak=bool(law.get('peak', 0)))
            o = Mo[bi] * np.sin(k * g * s_in) / k + others
            return abs(np.dot(o[ss], e)) / pd
        g0 = gain(0.0)
        try: att = brentq(lambda q: gain(q) - ref, 0.0, 1.5, xtol=1e-7) if g0 > ref else 0.0
        except ValueError: att = float('nan')
        out.append((f, 20 * np.log10(ref), 20 * np.log10(g0), 20 * np.log10(att + 1e-12), 20 * np.log10(fir_mag(bi, f))))
        print(f'{f:6d} Hz  probe gain ref {out[-1][1]:7.3f} (alone {out[-1][2]:7.3f})   heard {out[-1][3]:7.1f} dB   FIR {out[-1][4]:6.1f} dB', flush=True)
    return out


if __name__ == '__main__':
    band = sys.argv[1]
    run(band, float(sys.argv[2]) if len(sys.argv) > 2 else -2.0, int(sys.argv[3]) if len(sys.argv) > 3 else 4)
