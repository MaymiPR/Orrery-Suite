"""The visualiser stripe: fits its glow to a film of the reference and compares a film of ours with it.

    python fit_stripe.py orion.raw                 fits the glow, writes Source/ui/StripeGlow.h
    python fit_stripe.py orion.raw ours.raw        prints the statistics the particles were matched on, for both

The films: tools/RefCapture at 200 % (Orion's state with prefs.uiScaling=2, nulltest/orionstate.py; ours with
uiScale="2.0"), rows=0.66,0.775 (the stripe's band), in= the probe signal below (1 kHz tone, mono float32 48 kHz):
1.5 s silence, then with 1.5 s of silence after each: -60 dB 2 s then -10 dB 2 s; -10 then -60; -60 1.5 s, 0.3 s gap,
-10 1.5 s; -55, -50, -45, -40, -35 dB 1.5 s each; -60 then -40 2 s each; -60 1 s, a 0.2 s rise to -10, -10 1.8 s.
WINDOWS are the stretches where the reference's stripe is fully lit; its frame at 0.5 s is the idle stripe.
"""
import os
import sys
import numpy as np
from scipy import ndimage
from scipy.optimize import least_squares

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'TheAntiparticle', 'nulltest'))
from filmtools import Film  # noqa: E402

WINDOWS = [(3.8, 5.4), (7.2, 9.5), (14.6, 15.7), (26.5, 27.7), (29.5, 30.7), (34.5, 36.2), (39.0, 40.7)]
STOPS = [5.5, 9.0, 11.0, 15.8, 18.8, 21.8, 24.8, 27.8, 30.8, 36.3, 40.8]   # where the tone stops or drops
BAND_TOP = 266.5                       # design y of the film's first row (2 rows per design px)


def frames(f):
    return np.stack([f.frame(i) for i in range(f.n) if any(a < f.t[i] < b for a, b in WINDOWS)]).astype(np.float32)


def fit_glow(f):
    """the 20th percentile of the lit frames less the idle frame, as a profile down the stripe times a bell along it
    per channel"""
    idle = f.at(0.5).astype(np.float32)
    d = np.clip(np.percentile(frames(f), 20, axis=0) - idle, 0, None)[11:82, 20:1756]
    xs = (np.arange(20, 1756) + 0.5) / 2
    ys = (np.arange(11, 82) + 0.5) / 2 + BAND_TOP
    U, s, Vt = np.linalg.svd(d.mean(-1), full_matrices=False)
    Y = U[:, 0] * np.sign(U[:, 0].sum()); Y = Y / Y.max()
    bell = lambda p, x: p[1] * np.exp(-((x - p[0]) / p[2]) ** 2) + p[3] * np.exp(-np.abs(x - p[0]) / p[4])
    fits = []
    for c in range(3):
        X = (d[..., c] * Y[:, None]).sum(0) / (Y ** 2).sum()
        fits.append(least_squares(lambda p: bell(p, xs) - X, [460, 30, 120, 30, 80]).x)
    model = np.stack([Y[:, None] * bell(fits[c], xs)[None, :] for c in range(3)], -1)
    print('glow fit rms %.2f (mean %.2f)' % (np.sqrt(((model - d) ** 2).mean()), d.mean()))
    f3 = lambda v: ', '.join('%.3ff' % x for x in v)
    out = ['#pragma once',
           '// The glow behind the stripe\'s particles, fitted to the reference while it plays (plugins/Scorpius/tools/fit_stripe.py:',
           '// the 20th percentile of its frames at full brightness, less its idle frame). Separable: a profile down the stripe',
           '// (rows 0.5 design px apart) times, per colour channel, a bell along it: a exp (-((x - c) / s)^2) + b exp (-|x - c| / t),',
           '// in grey levels (0 ... 255) at full brightness.',
           'namespace sc::stripeglow', '{',
           'constexpr float rowsFrom = %.2ff, rowStep = 0.5f;' % ys[0], 'constexpr int numRows = %d;' % len(Y),
           'constexpr float rows[numRows] {'] + ['    %s,' % f3(Y[i:i + 12]) for i in range(0, len(Y), 12)] + ['};',
           'struct Bell { float c, a, s, b, t; };', 'constexpr Bell along[3] {   // red, green, blue'] + \
          ['    { %s },' % f3(p) for p in fits] + ['};', '}']
    path = os.path.join(HERE, '..', 'Source', 'ui', 'StripeGlow.h')
    open(path, 'w', newline='\n').write('\n'.join(out) + '\n')
    print('wrote', path)


def stats(name, f):
    st = frames(f)
    L = st.mean(-1)
    idle = f.at(0.5).astype(np.float32).mean(-1)
    p20 = np.percentile(L, 20, axis=0)
    mean = L.mean(0)
    rows = lambda img: ' '.join('%3.0f' % img[y:y + 4, 100:1676].mean() for y in range(10, 84, 4))
    cols = lambda img: ' '.join('%3.0f' % img[15:80, x:x + 74].mean() for x in range(0, 1776, 74))
    print('==', name, '(%d lit frames)' % len(st))
    print(' glow (p20 - idle) by rows ', rows(p20 - idle))
    print(' particles (mean - p20) rows', rows(mean - p20))
    print(' glow along                ', cols(p20 - idle))
    print(' particles along           ', cols(mean - p20))
    # particles per frame (local maxima 30 above the 20th percentile) by 2 design px across the stripe
    count = np.zeros(23); peaks = [[] for _ in range(23)]
    for k in range(0, len(L), 2):
        d = np.clip(L[k] - p20, 0, None)
        ys, xs = np.nonzero((d == ndimage.maximum_filter(d, size=3)) & (d > 30))
        for y, v in zip(ys, d[ys, xs]): count[y // 4] += 1; peaks[y // 4].append(v)
    n = len(range(0, len(L), 2))
    print(' particles/frame by rows   ', ' '.join('%3.0f' % (c / n) for c in count))
    print(' their median peak         ', ' '.join('%3.0f' % (np.median(p) if p else 0) for p in peaks))
    # how fast the particle layer changes (frame-to-frame correlation)
    part = L[:, 15:80, 100:1676] - p20[15:80, 100:1676]
    print(' particle layer correlation', ' '.join('lag %d: %.2f' % (lag, np.mean([np.corrcoef(part[k].ravel(), part[k + lag].ravel())[0, 1]
                                                                            for k in range(0, len(part) - lag, 2)])) for lag in (1, 2, 4, 10)))
    # brightness after each stop (0 idle, 1 lit)
    full = np.mean([f.at(t)[22:78, 30:1740].astype(float).mean() for t in np.arange(7.5, 9.0, 0.1)])
    base = f.at(0.5)[22:78, 30:1740].astype(float).mean()
    b = lambda t: (f.at(t)[22:78, 30:1740].astype(float).mean() - base) / (full - base)
    for e in STOPS:
        print(' after %4.1f s:' % e, ' '.join('%.2f' % b(e + dt) for dt in np.arange(0, 1.3, 0.15)))


if __name__ == '__main__':
    ref = Film(sys.argv[1])
    if len(sys.argv) == 2:
        fit_glow(ref)
    else:
        stats('reference', ref)
        stats('ours', Film(sys.argv[2]))
