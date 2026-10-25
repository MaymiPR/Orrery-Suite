"""The whole character section in Python (stereo): FIR bands and what their nonlinear stages hear (ident), each band's
gain computer (charsim, mid signal), shaper per channel with its dry share, make-up and side gain. Same structure as
Source/dsp/Character.h; used to fit band laws against program material without rendering the plug-in.

    import charmodel as M
    y = M.render(x, node, laws)          # x (2, n); laws {'low': law, 'mid': law, 'high': law} as in bandlaw.json"""
import json, os
import numpy as np
import tgp, ident as I, charsim as C

FS = tgp.FS
WIDTH = json.load(open(os.path.join(tgp.ROOT, 'measurements', 'character', 'width.json')))
NAMES = ['low', 'mid', 'high']


def prep(x):
    """band and nonlinear-stage signals of a stereo input: (3, 2, n) each"""
    return I.bands(x), I.nl_bands(x)


def det_hp(x, fc):
    """a first-order highpass at fc, 0 dB at 8 kHz (experiment: a detector-only low edge, detedge.py)"""
    from scipy.signal import lfilter
    k = np.tan(np.pi * fc / FS); b = np.array([1, -1]) / (1 + k); a = np.array([1, (k - 1) / (1 + k)])
    z = np.exp(-2j * np.pi * 8000 / FS); g8 = abs((b[0] + b[1] * z) / (a[0] + a[1] * z))
    return lfilter(b / g8, a, x)


def band(xb, sb, law, makeup_db, width_db):
    """one band's stereo output"""
    p = {k: v for k, v in law.items() if k not in ('k', 'residual_dB', 'median_abs_err', 'peak', 'shapeMid', 'detHp')}
    m, sd = 0.5 * (sb[0] + sb[1]), 0.5 * (sb[0] - sb[1])
    if law.get('detHp', 0) > 1: m, sd = det_hp(m, law['detHp']), det_hp(sd, law['detHp'])
    g = C.band_gain(m, p, side=sd, peak=int(law.get('peak', 0)))
    d = law.get('shapeDry', 0.0); k = law['k']
    u = g * sb; e = (1 - d) * (np.sin(k * u) / k - u)      # the shaper's distortion
    y = xb - sb + u + e
    M = 10 ** (makeup_db / 20); W = 10 ** (width_db / 20)
    m, s = (0.5 * (y[0] + y[1]) + 0.5 * law.get('shapeMid', 0.0) * (e[0] + e[1])) * M, 0.5 * (y[0] - y[1]) * M * W
    return np.stack([m + s, m - s])


def render(x, node, laws, pre=None):
    xb, sb = pre if pre is not None else prep(x)
    k17 = int(round(node * 2))
    return sum(band(xb[b], sb[b], laws[NAMES[b]], I.GAINS16[b][k17], WIDTH[NAMES[b]][k17]) for b in range(3))
