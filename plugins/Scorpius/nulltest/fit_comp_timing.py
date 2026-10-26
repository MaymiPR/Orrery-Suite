"""The compressor's detector timing, fitted on what the static fit (fit_comp.py) leaves: level steps, loud low tones,
drums and the track (MUSIC=1: drums and the track only). With the gain applied before the wet filters (as the
reference: loud drums -30.5 -> -47.5 dB) the rest is small (MEASUREMENTS.md 14).
    python fit_comp_timing.py [a|b] [param=start ...]     params: attack release rms gAttack gRelease thresh"""
import sys
import numpy as np
from scipy.optimize import minimize
import orion as o
import nullmod as N

FS = o.FS
mode = sys.argv[1] if len(sys.argv) > 1 else 'b'
M = 'compA' if mode == 'a' else 'compB'
hidden = {'ids.compression.mode': 'Mode A' if mode == 'a' else 'Mode B'}
start = dict(a.split('=') for a in sys.argv[2:]) or {'attack': '1.0', 'release': '19.3', 'rms': '0.05'}
NAMES = list(start)
t = np.arange(int(1.5 * FS)) / FS


def steps(f):
    seg = int(0.3 * FS)
    x = np.concatenate([10 ** (db / 20) * np.sin(2 * np.pi * f * np.arange(seg) / FS) for db in (-30, 0, -30, -6, 0, -20, 6, -30)])
    return np.stack([x, x]).astype(np.float32)


sig = N.signals()
X = {'steps 1k': steps(1000)}
for f in (25, 40, 60):
    for db in (0, -6):
        X[f'{f} Hz {db}'] = np.stack([10 ** (db / 20) * np.sin(2 * np.pi * f * t)] * 2).astype(np.float32)
X.update({k: (np.stack([v, v]) if v.ndim == 1 else v) for k, v in sig.items()})
import os
if os.environ.get('MUSIC'): X = {k: v for k, v in X.items() if k in sig}      # (music only)
jobs = [dict(x=x, params=o.ONLY('comp'), hidden=hidden) for x in X.values()]
R = o.render_many(jobs, 'ref', cache=True)


def nulls(v):
    tune = {f'{M}.{k}': float(q) for k, q in zip(NAMES, v)}
    Y = o.render_many([dict(j, tune=tune) for j in jobs], 'model')
    return [o.null_db(r, y) for r, y in zip(R, Y)]


if __name__ == '__main__':
    print(list(X))
    print('now ', np.round(nulls([float(v) for v in start.values()]), 1), flush=True)
    v0 = [float(v) for v in start.values()]
    r = minimize(lambda v: float(np.mean(nulls(v))), v0, method='Nelder-Mead', options=dict(maxiter=100, xatol=1e-3, fatol=1e-3))
    print('fit ', dict(zip(NAMES, np.round(r.x, 4))), np.round(nulls(r.x), 1), flush=True)
