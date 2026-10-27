"""The rate converters above 48 kHz: the half lengths of the input and output filters (within the reference's delay at
every rate), their Kaiser beta, the input and output cutoffs (shares of 24 kHz) and a delay trim added to the
reference's measured delay, fitted on drums and the track with every module off at 88.2 / 96 / 176.4 / 192 kHz
(MEASUREMENTS.md 2, 14).
    python fit_resample_high.py"""
import numpy as np
import scipy.signal as sg
from scipy.optimize import minimize
import orion as o
import nullmod as N

sig = N.signals()
RATES = {88200: (147, 80), 96000: (2, 1), 176400: (147, 40), 192000: (4, 1)}
JOBS, REF = [], []
for fs, (up, dn) in RATES.items():
    for k in ('drums', 'track'):
        v = sig[k]; v = v if v.ndim > 1 else np.stack([v, v])
        x = sg.resample_poly(v[:, :4 * 48000], up, dn, axis=1).astype(np.float32)
        JOBS.append((fs, dict(x=x, params=o.NONE)))
        REF.append(o.render(x, 'ref', fs=fs, cache=True, params=o.NONE))
NAMES = ['resampleDelayTrimHigh', 'resampleCutoffInHigh', 'resampleCutoffOutHigh', 'resampleBetaHigh']
HALVES = {}


def nulls(v):
    tune = {**dict(zip(NAMES, map(float, v))), **HALVES}
    return [o.null_db(r, o.render(j['x'], 'model', fs=fs, params=j['params'], tune=tune)) for (fs, j), r in zip(JOBS, REF)]


if __name__ == '__main__':
    for h_in, h_out in ((16, 16), (24, 12), (32, 8), (38, 4)):
        HALVES.update(resampleHalfInHigh=h_in, resampleHalfOutHigh=h_out)
        r = minimize(lambda v: float(np.mean(nulls(v))), [0.2, 0.95, 0.95, 8.0], method='Nelder-Mead', options=dict(maxiter=60, xatol=1e-3, fatol=1e-2))
        print(f'halves {h_in} / {h_out}:', dict(zip(NAMES, np.round(r.x, 4))), np.round(nulls(r.x), 1), flush=True)
