"""Limiter probes (limiter stage alone, its input gain at 0 dB so levels are re the ceiling): a small DC pilot keeps the
gain measurable at every sample (DC passes the core path's all-pass unchanged), smooth Hann bumps of known height
(single ones, and pairs at set spacings) show how the gain ramps toward and away from peaks. The gain is the output
over the input as it reaches the limiter (the core path is exactly a fixed delay plus all-pass, MEASUREMENTS.md 1)."""
import numpy as np
import tgp

FS = tgp.FS
PRE = 2 * FS
PILOT = 0.01
WIDTH = 9
TRIM = 10 ** (-0.1 / 20)


def bump(h):
    return h * np.hanning(WIDTH + 2)[1:-1]


def events():
    """(kind, args, sample index of the first bump's centre)"""
    ev, t = [], PRE + FS
    for h in (1.2, 1.5, 2.0, 3.0, 5.0, 8.0):
        ev.append(('single', (h,), t)); t += 2 * FS
    for big, small in ((3.0, 1.6), (1.6, 3.0), (3.0, 3.0)):
        for d in (4, 8, 12, 16, 20, 24, 30, 40, 60):
            ev.append(('pair', (big, small, d), t)); t += 2 * FS
    return ev, t + FS


def signal():
    ev, n = events()
    x = np.full(n, PILOT)
    x[:PRE // 2] = 0
    for kind, args, t in ev:
        if kind == 'single':
            x[t - WIDTH // 2:t + WIDTH // 2 + 1] += bump(args[0])
        else:
            big, small, d = args
            x[t - WIDTH // 2:t + WIDTH // 2 + 1] += bump(big)
            x[t + d - WIDTH // 2:t + d + WIDTH // 2 + 1] += bump(small)
    return x.astype(np.float32)


def env(plugin='ref', tune=None):
    """detector level (1 / gain) at every sample, and the limiter's input level, both aligned with the output"""
    x = signal()
    params = {'Limiter Input Gain': 0}
    j = [dict(x=x, norm=tgp.LIM_ONLY, params=params), dict(x=x / 1000, norm=tgp.LIM_ONLY, params=params)]
    if plugin == 'model':
        j = [dict(jj, tune=tune or {}) for jj in j]
        y, q = tgp.render_many(j, 'model')
    else:
        y, q = tgp.render_many(j, cache=True)
    q = q[0] * 1000 / TRIM
    e = q / y[0] * TRIM
    return e, np.maximum(1.0, np.abs(q))


TRAINS = [(h, dur) for h in (2.0, 4.0) for dur in (0.01, 0.05, 0.2, 1.0)]
TRAIN_GAP = 60          # samples between bumps (longer than the 43-sample triangle, so the floor is visible between)


def train_signal():
    """trains of bumps (height, seconds), each followed by 2.5 s of pilot only; returns x and the train start / end indices"""
    t = PRE + FS; spans = []
    n = t + sum(int(d * FS) + int(2.5 * FS) for _, d in TRAINS) + FS
    x = np.full(n, PILOT); x[:PRE // 2] = 0
    for h, dur in TRAINS:
        end = t + int(dur * FS)
        for c in range(t, end, TRAIN_GAP):
            x[c - WIDTH // 2:c + WIDTH // 2 + 1] += bump(h)
        spans.append((h, dur, t, end)); t = end + int(2.5 * FS)
    return x.astype(np.float32), spans


def train_env(plugin='ref', tune=None):
    x, spans = train_signal()
    params = {'Limiter Input Gain': 0}
    j = [dict(x=x, norm=tgp.LIM_ONLY, params=params), dict(x=x / 1000, norm=tgp.LIM_ONLY, params=params)]
    if plugin == 'model':
        y, q = tgp.render_many([dict(jj, tune=tune or {}) for jj in j], 'model')
    else:
        y, q = tgp.render_many(j, cache=True)
    q = q[0] * 1000 / TRIM
    return q / y[0] * TRIM, spans
