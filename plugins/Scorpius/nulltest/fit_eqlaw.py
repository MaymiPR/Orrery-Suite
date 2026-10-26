"""Harmonic EQ: the knob's law (Tuning::eqLaw), each point fitted by nulling the 1 kHz and 100 Hz bands at that knob
value against the reference (drums at -12 dBFS: below the soft clip's reach, so the gain alone counts)."""
import numpy as np
from scipy.optimize import minimize_scalar
import orion as o
from order import drums

x = drums() * 10 ** (-12 / 20)
KNOB = [0.5, 1, 2, 3, 4, 5, 6]
EQ = lambda g: [{**o.ONLY('eq'), 'EQ 1kHz': g}, {**o.ONLY('eq'), 'EQ 100Hz': g}]
law = [0, 0.634, 1.258, 2.482, 3.677, 4.846, 5.994, 7.124]
for i, g in enumerate(KNOB, start=1):
    jobs = [dict(x=x, params=p) for p in EQ(g)]
    ref = o.render_many(jobs, cache=True)
    def cost(v):
        ours = o.render_many([dict(j, tune={f'eqLaw{i}': v}) for j in jobs], plugin='model')
        return np.mean([o.null_db(r, m) for r, m in zip(ref, ours)])
    s = minimize_scalar(cost, bracket=(law[i] - 0.05, law[i] + 0.05), tol=1e-4)
    law[i] = s.x
    print(f'knob {g}: {s.x:.4f} dB (null {s.fun:.1f})', flush=True)
print('eqLaw', [round(v, 4) for v in law])
