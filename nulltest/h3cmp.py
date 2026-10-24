"""In phase against anti-phase through the high band, below its threshold: the fundamental's gain and the 3rd harmonic
re A^3 (the shaper's cubic term) at a few levels. If the shaper is the same for both and the in-phase tone is not
compressed, both agree; a gain reduction below threshold changes only the fundamental.

    python h3cmp.py [amount 0..1] [freq Hz] [band]"""
import json, os, sys
import numpy as np
import tgp, ident as I

FS = tgp.FS
LEVELS = [-30, -24, -18]

if __name__ == '__main__':
    amt = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5
    f0 = float(sys.argv[2]) if len(sys.argv) > 2 else 7000.0
    band = sys.argv[3] if len(sys.argv) > 3 else 'high'
    node = int(round(amt * 16))
    W = json.load(open(os.path.join(tgp.ROOT, 'measurements', 'character', 'width.json')))[band][node]
    n = FS * 2; t = np.arange(n) / FS
    for who in ('ref', 'model'):
        rows = []
        for l in LEVELS:
            x = np.concatenate([np.zeros(I.PRE), 10 ** (l / 20) * np.sin(2 * np.pi * f0 * t)])
            out = []
            for sgn in (1, -1):
                j = dict(x=np.stack([x, sgn * x]).astype(np.float32), norm={**tgp.CHAR_ONLY, 'Character': amt}, raw=True)
                y = tgp.render_many([j], who, cache=who == 'ref')[0]
                v = 0.5 * (y[0] + sgn * y[1])                      # mid for in phase, side for anti-phase
                if sgn < 0: v = v / 10 ** (W / 20)
                seg = v[-FS:]; w = np.hanning(FS); tt = np.arange(FS) / FS
                h = lambda f: 2 * abs(np.sum(seg * w * np.exp(-2j * np.pi * f * tt))) / np.sum(w)
                A = 10 ** (l / 20)
                out.append((h(f0) / A, h(3 * f0) / A ** 3))
            rows.append(out)
        print(who)
        for l, ((g1, c1), (g2, c2)) in zip(LEVELS, rows):
            print(f'  {l:+d} dBFS  fundamental in/anti {20 * np.log10(g1 / g2):+.5f} dB   H3/A^3 in {c1:.5f} anti {c2:.5f}  ratio {c1 / c2:.4f}')
