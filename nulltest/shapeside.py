"""The high band's shaper curve, read sample by sample: a tone with left and right in anti-phase is not compressed (the
detectors hear the mid only, MEASUREMENTS.md 3.10), so the band's side output is M W F(s) with s its band signal.
The other bands are taken off as linear (they hear the tone 26 dB down or less); a slow level ramp sweeps u over the
shaper's range. Fits sin(k u) / k and an odd polynomial to the curve, reference and ours.

    python shapeside.py [amount 0..1] [freq Hz] [band 0..2] [only=ref]
Prints a JSON line 'SHAPER band amount k d residual' for each plug-in."""
import sys
import numpy as np
import tgp, ident as I

FS = tgp.FS

if __name__ == '__main__':
    amt = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5
    pos = [a for a in sys.argv[1:] if '=' not in a]
    f0 = float(pos[1]) if len(pos) > 1 else 9000.0
    band = int(pos[2]) if len(pos) > 2 else 2
    only = dict(a.split('=') for a in sys.argv[1:] if '=' in a).get('only')
    node = int(round(amt * 16))
    n = I.PRE + 4 * FS; t = np.arange(n) / FS
    lev = np.interp(t, [I.PRE / FS, n / FS], [-24.0, 3.0]); lev[:I.PRE] = -24.0
    x = np.sin(2 * np.pi * f0 * t) * 10 ** (lev / 20); x[:FS // 2] = 0
    stereo = np.stack([x, -x]).astype(np.float32)
    import json, os
    W = json.load(open(os.path.join(tgp.ROOT, 'measurements', 'character', 'width.json')))
    Wb = [10 ** (W[b][node] / 20) for b in I.BANDS] if hasattr(I, 'BANDS') else [10 ** (W[b][node] / 20) for b in ('low', 'mid', 'high')]
    M = [10 ** (I.GAINS16[b][node] / 20) for b in range(3)]
    xb = I.bands(x)[:, 0]
    seg = slice(I.PRE + FS // 4, n - FS // 100)
    for who in ([only] if only else ('ref', 'model')):
        y = tgp.render_many([dict(x=stereo, norm={**tgp.CHAR_ONLY, 'Character': amt}, raw=True)], who, cache=who == 'ref')[0]
        side = 0.5 * (y[0] - y[1])
        yh = (side - sum(M[b] * Wb[b] * xb[b] for b in range(3) if b != band)) / (M[band] * Wb[band])
        s, v = I.nl_bands(x)[band, 0][seg], (yh - xb[band] + I.nl_bands(x)[band, 0])[seg]
        # sin(k u) / k with a free scale g
        best = None
        for k in np.linspace(1.0, 3.0, 2001):
            b = np.sin(k * s) / k; g = np.dot(b, v) / np.dot(b, b); e = np.sum((v - g * b) ** 2)
            if best is None or e < best[0]: best = (e, k, g)
        e, k, g = best
        # d u + (1 - d) sin(k u) / k with a free scale: linear in (g d, g (1 - d)) for each k
        bestd = None
        for kk in np.linspace(0.05, 3.5, 3451):
            B = np.stack([s, np.sin(kk * s) / kk], 1); cc, *_ = np.linalg.lstsq(B, v, rcond=None)
            ee = np.sum((B @ cc - v) ** 2)
            if bestd is None or ee < bestd[0]: bestd = (ee, kk, cc)
        ed, kd, cd = bestd; gd = cd.sum(); dd = cd[0] / gd
        print(f'{who}: dry-share fit k {kd:.4f} d {dd:+.4f} scale {20 * np.log10(gd):+.3f} dB residual {10 * np.log10(ed / np.sum(v ** 2)):.1f} dB')
        print(f'SHAPER {band} {amt} {kd:.5f} {dd:.5f} {10 * np.log10(ed / np.sum(v ** 2)):.1f} {who}')
        X = np.stack([s ** p for p in (1, 3, 5, 7, 9)], 1); c, *_ = np.linalg.lstsq(X, v, rcond=None)
        ep = np.sum((X @ c - v) ** 2)
        print(f'{who}: sine k {k:.4f} scale {20 * np.log10(g):+.3f} dB  residual {10 * np.log10(e / np.sum(v ** 2)):.1f} dB;  '
              f'poly {np.round(c / c[0], 5)} residual {10 * np.log10(ep / np.sum(v ** 2)):.1f} dB;  peak |s| {np.max(np.abs(s)):.3f}')
        # the curve's error against the fitted sine, by |s|
        edges = np.linspace(0, np.max(np.abs(s)), 9)
        r = v - g * np.sin(k * s) / k
        print('   |s| bins: ' + '  '.join(f'{edges[i + 1]:.2f}:{np.sqrt(np.mean(r[(np.abs(s) >= edges[i]) & (np.abs(s) < edges[i + 1])] ** 2)):.1e}' for i in range(8)))
        np.save(os.path.join(tgp.ROOT, 'measurements', 'character', f'shapeside_{who}_{amt}_{int(f0)}.npy'), np.stack([s, v]))
