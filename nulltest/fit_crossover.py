"""Fits the character section's band split to the masker / probe data (measurements/character/dyn.json): probe gain =
|sum_b g_b H_b(f)| for a 3-band Linkwitz-Riley crossover (low = LP(fc1) * AP(fc2), mid = HP(fc1) * LP(fc2),
high = HP(fc1) * HP(fc2)), with free fc1, fc2, order (2 or 4) and per-masker band gains g_b. Probes hit by the masker's
intermodulation products are down-weighted (soft-L1 loss). Writes measurements/character/crossover_fit.json."""
import json, os
import numpy as np
from scipy.optimize import least_squares
import tgp

D = json.load(open(os.path.join(tgp.ROOT, 'measurements', 'character', 'dyn.json')))['bands']
f = np.array(D['probes_hz'], float)
masks = {int(k): np.array(v) for k, v in D['probe_gain_re_alone_dB'].items()}


def lr(fc, order):
    s = 1j * f / fc
    if order == 2:
        lp = 1 / (1 + s) ** 2; hp = s ** 2 / (1 + s) ** 2       # LR2: high output inverted to sum flat (allpass)
        return lp, -hp
    b = 1 / (s ** 2 + np.sqrt(2) * s + 1)
    return b * b, (s ** 2 * b) ** 2


def bands(fc1, fc2, order):
    l1, h1 = lr(fc1, order); l2, h2 = lr(fc2, order)
    return l1 * (l2 + h2), h1 * l2, h1 * h2


def fit(order):
    ms = sorted(masks)
    def resid(p):
        L, M, H = bands(p[0], p[1], order); r = []
        for i, m in enumerate(ms):
            g = 10 ** (p[2 + 3 * i: 5 + 3 * i] / 20)
            pred = 20 * np.log10(np.abs(g[0] * L + g[1] * M + g[2] * H) / np.abs(L + M + H))
            e = pred - masks[m]
            r.append(e)
        return np.concatenate(r)
    p0 = [160, 2700] + [0, 0, 0] * len(ms)
    lo = [40, 1000] + [-30] * 3 * len(ms); hi = [600, 8000] + [10] * 3 * len(ms)
    return least_squares(resid, p0, loss='soft_l1', f_scale=0.05, bounds=(lo, hi)), ms   # robust: probes hit by IMD don't drag the fit


out = {}
for order in (2, 4):
    r, ms = fit(order)
    rms = float(np.median(np.abs(r.fun)))
    out[f'LR{order}'] = dict(fc1_hz=round(float(r.x[0]), 1), fc2_hz=round(float(r.x[1]), 1), median_abs_err_dB=round(rms, 3),
                             band_gain_dB={str(m): np.round(r.x[2 + 3 * i: 5 + 3 * i], 2).tolist() for i, m in enumerate(ms)})
    print(order, out[f'LR{order}'])
json.dump(out, open(os.path.join(tgp.ROOT, 'measurements', 'character', 'crossover_fit.json'), 'w'), indent=1)
