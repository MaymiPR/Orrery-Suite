"""Harmonics at amount 1 (the wet path), simulated in Python and nulled against the reference: the linear part of
fit_harmlin.py (an RBJ bell whose boost passes a 20 Hz high-pass) with the boost saturated before it is added,
W = x + HP(c S(boost / c)), S(u) = u / (1 + |u|^p)^(1/p); or a structure given by `form`.
    python fit_harm.py"""
import numpy as np
from scipy.optimize import minimize
from scipy.signal import butter, sosfilt, lfilter
import orion as o
import nullmod as N
from fit_eqrbj import rbj

FS = 48000
LIN = {80: (112.221, 0.3204, 5.851, 20.372), 50: (69.928, 0.3194, 5.855, 20.301)}

def shape(u, p): return u / (1 + np.abs(u) ** p) ** (1 / p)

def wet(x, fr, c, p, form='boost'):
    f0, q, g, hp = LIN[fr]
    b, a = rbj('peak', f0, q, g)
    boost = lfilter(b, a, x) - x
    hps = butter(2, hp, 'high', fs=FS, output='sos')
    if form == 'boost':          # the boost saturated, then the high-pass
        return x + sosfilt(hps, c * shape(boost / c, p))
    if form == 'whole':          # the whole wet signal saturated
        y = x + sosfilt(hps, boost)
        return c * shape(y / c, p)
    raise ValueError(form)

sig = N.signals()
xs = [v if v.ndim == 1 else v[0] for v in sig.values()]
tones = [(fr_, lv) for fr_ in (40, 100, 300) for lv in (-12, -6, 0)]
txs = [o.tone(f_, 1.0, lv) for f_, lv in tones]

def refs(fr):
    p = {**o.ONLY('harm'), 'Harm Amount': 1, 'Harm Freq': fr}
    return o.render_many([dict(x=x, params=p) for x in xs + txs], cache=True)

def nulls(fr, c, p, form):
    R = refs(fr)
    return [o.null_db(r[:1], wet(x.astype(np.float64), fr, c, p, form)[None]) for x, r in zip(xs + txs, R)]

if __name__ == '__main__':
    for form in ('boost', 'whole'):
        for fr in (80,):
            cost = lambda v: np.mean(nulls(fr, v[0], v[1], form))
            s = minimize(cost, [0.5, 2.0], method='Nelder-Mead', options=dict(xatol=1e-3, fatol=0.02, maxiter=120))
            print(form, fr, 'c %.4f p %.3f' % tuple(s.x), 'nulls', ' '.join('%.1f' % v for v in nulls(fr, *s.x, form)), flush=True)
