"""The character section's stereo width: each band's side gain relative to its mid, per amount node.

Low-level white noise in the left channel only (the compressors stay below their knees, the shapers are linear): the
outputs are then L = sum_b (g_mid + g_side) / 2 * x_b and R = sum_b (g_mid - g_side) / 2 * x_b over the exactly known
band signals x_b (BandFilters.h), so a least-squares fit per channel gives both gains of every band (the residual is
-96 dB or better). The side gain is applied after the shaper (a left-only tone at 200 % fits to -45 dB that way, -32 dB
with it before). Writes measurements/character/width.json: {band: [side - mid dB at 0, 12.5, ..., 200 %]}."""
import json, os
import numpy as np
import tgp, ident as I

OUT = os.path.join(tgp.ROOT, 'measurements', 'character', 'width.json')

if __name__ == '__main__':
    rng = np.random.default_rng(5)
    n, pre = 4 * tgp.FS, 2 * tgp.FS
    x = np.concatenate([np.zeros(pre), rng.standard_normal(n) * 10 ** (-50 / 20)]).astype(np.float32)
    ys = tgp.render_many([dict(x=np.stack([x, 0 * x]), norm={**tgp.CHAR_ONLY, 'Character': k / 16}, raw=True) for k in range(17)], cache=True)
    A = I.bands(x[pre:])[:, 0].T
    res = {b: [] for b in ('low', 'mid', 'high')}
    for k, y in enumerate(ys):
        cl, *_ = np.linalg.lstsq(A, y[0][pre:], rcond=None)
        cr, *_ = np.linalg.lstsq(A, y[1][pre:], rcond=None)
        for i, b in enumerate(res):
            res[b].append(round(float(20 * np.log10((cl[i] - cr[i]) / (cl[i] + cr[i]))), 4))
    print(res)
    json.dump(res, open(OUT, 'w'), indent=1)
