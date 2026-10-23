"""Fits the limiter's constants (Source/dsp/Limiter.h) to the reference: limiter-only renders of DC steps, loud pink
noise, drums and a dense mix; minimises the summed residual energy (reference - ours, re each reference) with
Nelder-Mead. Prints the best tuning lines for Source/dsp/Tuning.h and writes measurements/limiter/fit.json."""
import json, os
import numpy as np
from scipy.optimize import minimize
import tgp, nulltest as NT
import measure_limiter as ML

FS = tgp.FS
PRE = 2 * FS
KEYS = ['limSlopeAhead', 'limSlopeBehind', 'limHoldAhead', 'limHoldBehind', 'limSlowAttackMs', 'limSlowReleaseMs']
X0 = [0.17, 0.2, 0.0, 0.0, 24.5, 377.8]


def cases():
    pad = lambda x: np.concatenate([np.zeros((2, PRE) if x.ndim == 2 else PRE, np.float32), x], axis=-1)
    out = [dict(x=ML.step_sig(*s), norm=tgp.LIM_ONLY) for s in ML.STEPS]
    out += [dict(x=pad(NT.sig_noise(-6)), norm=tgp.LIM_ONLY), dict(x=pad(NT.sig_drums(-6)), norm=tgp.LIM_ONLY, params={'Limiter Input Gain': 12}),
            dict(x=pad(NT.sig_music(-2)), norm=tgp.LIM_ONLY), dict(x=pad(NT.sig_music(-8)), norm=tgp.LIM_ONLY, params={'Limiter Input Gain': 12})]
    return out


def residuals(jobs, ref, tune):
    ys = tgp.render_many([dict(j, tune=tune) for j in jobs], 'model')
    return [NT.residual_db(r, y) for r, y in zip(ref, ys)]


if __name__ == '__main__':
    jobs = cases()
    ref = tgp.render_many(jobs, 'ref', cache=True)
    start = residuals(jobs, ref, dict(zip(KEYS, X0)))
    print('start', np.round(start, 2))

    def cost(x):
        r = residuals(jobs, ref, dict(zip(KEYS, x)))
        c = float(np.mean([10 ** (v / 10) for v in r]))      # mean residual energy: the worst cases dominate
        print(np.round(x, 4), np.round(r, 2), round(10 * np.log10(c), 3), flush=True)
        return c

    best = minimize(cost, X0, method='Nelder-Mead', options=dict(maxfev=220, xatol=1e-3, fatol=1e-9, initial_simplex=None))
    x = best.x
    end = residuals(jobs, ref, dict(zip(KEYS, x)))
    print('best', dict(zip(KEYS, np.round(x, 4))), 'residuals', np.round(end, 2))
    json.dump(dict(tuning=dict(zip(KEYS, [float(v) for v in x])), residual_dB_before=[round(float(v), 2) for v in start],
                   residual_dB_after=[round(float(v), 2) for v in end]), open(os.path.join(tgp.ROOT, 'measurements', 'limiter', 'fit.json'), 'w'), indent=1)
