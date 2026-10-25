"""Limiter pair probes at spacings around the window (21 at 48 kHz): what the envelope does in the samples after the
first peak when the next one is just out of reach. Reference against ours.

    python limpairs2.py [tune key=value ...]"""
import sys
import numpy as np
import tgp, lim_probe as L

SPACINGS = [21, 22, 23, 24, 25, 26, 28, 30]
PAIRS = [(3.0, 3.0), (1.6, 3.0), (3.0, 1.6), (3.0, 2.0), (2.0, 3.0)]


def signal():
    ev, t = [], L.PRE + L.FS
    for big, small in PAIRS:
        for d in SPACINGS:
            ev.append((big, small, d, t)); t += L.FS // 2
    x = np.full(t + L.FS, L.PILOT); x[:L.PRE // 2] = 0
    for big, small, d, t0 in ev:
        x[t0 - L.WIDTH // 2:t0 + L.WIDTH // 2 + 1] += L.bump(big)
        x[t0 + d - L.WIDTH // 2:t0 + d + L.WIDTH // 2 + 1] += L.bump(small)
    return x.astype(np.float32), ev


def env(x, plugin, tune=None):
    params = {'Limiter Input Gain': 0}
    j = [dict(x=x, norm=tgp.LIM_ONLY, params=params), dict(x=x / 1000, norm=tgp.LIM_ONLY, params=params)]
    if plugin == 'model': y, q = tgp.render_many([dict(jj, tune=tune or {}) for jj in j], 'model')
    else: y, q = tgp.render_many(j, cache=True)
    q = q[0] * 1000 / L.TRIM
    return q / y[0] * L.TRIM


if __name__ == '__main__':
    tune = dict(kv.split('=') for kv in sys.argv[1:]); tune = {k: float(v) for k, v in tune.items()}
    x, ev = signal()
    er, em = env(x, 'ref'), env(x, 'model', tune)
    worst = 0
    for big, small, d, t0 in ev:
        s = slice(t0 - 30, t0 + d + 30)
        dd = np.nanmax(np.abs(er[s] - em[s])); worst = max(worst, dd)
        print(f'({big}, {small}, {d:2d}) max diff {dd:.4f}   after the first peak: ref ' + ' '.join(f'{er[t0 + k]:.3f}' for k in range(0, 7))
              + '   ours ' + ' '.join(f'{em[t0 + k]:.3f}' for k in range(0, 7)))
    print('worst %.4f' % worst)
