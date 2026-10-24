"""The high band's shaper is stronger in the output's mid than in its side: tones below the threshold in phase and in
anti-phase, the 3rd harmonic re A^3 compared (h3cmp.py). The distortion part (1 - d)(sin(k u) / k - u) reaches the
output mid gamma times as strong as the output side; the anti-phase readings (shapeside.py) are the side's. Writes the
lines 'SHAPEMID row gamma' to measurements/character/shapemid_<band>.txt.

    python shapemid.py [band] [freq Hz]"""
import os, sys
import numpy as np
import tgp, ident as I

FS = tgp.FS

if __name__ == '__main__':
    band = sys.argv[1] if len(sys.argv) > 1 else 'high'
    f0 = float(sys.argv[2]) if len(sys.argv) > 2 else 7000.0
    n = FS * 2; t = np.arange(n) / FS; A = 10 ** (-24 / 20)
    x = np.concatenate([np.zeros(I.PRE), A * np.sin(2 * np.pi * f0 * t)])
    w = np.hanning(FS); tt = np.arange(FS) / FS
    h = lambda v: abs(np.sum(v[-FS:] * w * np.exp(-2j * np.pi * 3 * f0 * tt)))
    jobs = [dict(x=np.stack([x, s * x]).astype(np.float32), norm={**tgp.CHAR_ONLY, 'Character': row / 16}, raw=True) for row in range(1, 17) for s in (1, -1)]
    ys = tgp.render_many(jobs, 'ref', cache=True)
    lines = []
    for i, row in enumerate(range(1, 17)):
        yi, ya = ys[2 * i], ys[2 * i + 1]
        g = h(0.5 * (yi[0] + yi[1])) / h(0.5 * (ya[0] - ya[1]))      # H3 of in-phase mid / anti-phase side
        W = 10 ** (__import__('charmodel').WIDTH[band][row] / 20)
        lines.append(f'SHAPEMID {row} {g * W:.5f}')
        print(lines[-1], flush=True)
    open(os.path.join(tgp.ROOT, 'measurements', 'character', f'shapemid_{band}.txt'), 'w').write('\n'.join(lines) + '\n')
