"""What the high band's detector hears: a quiet 16 kHz probe (-40 dBFS) rides along with a steady sine pump at
frequency f; the probe's gain (hightrace.py's inverted-probe method) is the band's gain reduction caused by the pump,
so the gain reduction against f traces the detector's frequency response. Reference against ours.

    python detspec.py [amount 0..1] [pump dBFS]"""
import sys
import numpy as np
import tgp
from hightrace import FP, PROBE, gain_trace

FS = tgp.FS; PRE = FS
FREQS = [1000, 1500, 2000, 2400, 2700, 3000, 3500, 4000, 5000, 6000, 8000, 11000, 13000]

if __name__ == '__main__':
    amt = float(sys.argv[1]) if len(sys.argv) > 1 else 1.0
    lev = float(sys.argv[2]) if len(sys.argv) > 2 else -6.0
    n = PRE + 2 * FS; t = np.arange(n) / FS
    probe = PROBE * np.sin(2 * np.pi * FP * t)
    pumps = [np.sin(2 * np.pi * f * t) * 10 ** (lev / 20) for f in FREQS] + [0 * t]
    jobs = [dict(x=(p + s * probe).astype(np.float32), norm={**tgp.CHAR_ONLY, 'Character': amt}) for p in pumps for s in (1, -1)]
    rr = tgp.render_many(jobs, 'ref', cache=True); oo = tgp.render_many(jobs, 'model')
    seg = slice(PRE + FS // 2, n - FS // 4)
    g = lambda y, i: np.median(gain_trace(0.5 * (y[2 * i][0] - y[2 * i + 1][0]), n)[seg])
    br, bo = g(rr, len(FREQS)), g(oo, len(FREQS))
    print(f'amount {amt}, pump {lev} dBFS: probe gain re probe alone')
    for i, f in enumerate(FREQS):
        a, b = 20 * np.log10(g(rr, i) / br), 20 * np.log10(g(oo, i) / bo)
        print(f'{f:6d} Hz   ref {a:+7.3f}   ours {b:+7.3f}   diff {b - a:+.3f}', flush=True)
