"""Source/dsp/Limiter.h in Python at 48 kHz (the inter-sample levels vectorised, the plan logic per sample), for trying
plan rules on whole signals: env(x) gives the envelope of every outgoing sample; trace=(a, b) prints the decisions.
Rule variants are switched by keyword arguments (default: the plug-in's rule)."""
import numpy as np

LOOK, WINDOW, RELEASE, TAPS, SUB, BETA, REACH = 38, 21, 22, 16, 16, 6.0, 22.9
SLOW_ATT = 1 - np.exp(-1 / (24.47e-3 * 48000)); SLOW_REL = np.exp(-1 / (377.8e-3 * 48000))


TABLE = None          # an interpolator to try instead of the plug-in's (SUB x TAPS)


def interp_table():
    if TABLE is not None: return TABLE
    t = np.zeros((SUB, TAPS))
    for ph in range(SUB):
        x = np.arange(TAPS) - (TAPS // 2 - 1) - ph / SUB
        r = x / (TAPS / 2)
        w = np.where(np.abs(r) >= 1, 0, np.i0(BETA * np.sqrt(np.clip(1 - r * r, 0, None))) / np.i0(BETA))
        t[ph] = np.sinc(x) * w; t[ph] /= t[ph].sum()
    return t


def levels(x):
    """level[n] (>= 1): the inter-sample peak over [n, n + 1) of both channels; partial[n]: at the reach phase"""
    x = np.atleast_2d(x); T = interp_table(); n = x.shape[1]; taps = T.shape[1]
    ph_reach = int(np.floor((REACH - np.floor(REACH)) * SUB))
    m = np.zeros(n); mp = np.zeros(n)
    for c in range(x.shape[0]):
        xp = np.concatenate([np.zeros(taps), x[c]])
        for ph in range(T.shape[0]):
            # the C++ filter's output at input index i covers samples i - taps + 1 .. i; it is stored for i - taps / 2
            y = np.convolve(xp, T[ph][::-1], 'valid')[1:n + 1]      # y[i] = sum_j T[j] x[i - taps + 1 + j]
            v = np.zeros(n); v[:n - taps // 2] = np.abs(y[taps // 2:])
            m = np.maximum(m, v)
            if ph == ph_reach: mp = np.maximum(mp, v)
    return np.maximum(1, m), np.maximum(1, mp)


def env(x, trace=None, window=WINDOW, release=RELEASE, reach=REACH, rule=(), prom=0.0, states=False):
    """rule: 'sample' (peaks are local maxima of the sample magnitudes), 'wide2' (local maxima over two neighbours
    each side), 'steep' (a new plan heads for the peak in the window it can reach with the least descent);
    prom: a peak must stand this far above both neighbours' levels"""
    x = np.atleast_2d(x)
    lv, part = levels(x)
    n = len(lv)
    reach_whole = int(np.floor(reach))
    peak = np.ones(n)
    if 'sample' in rule:
        sa = np.max(np.abs(x), 0)
        loc = (sa[1:-1] >= sa[:-2]) & (sa[1:-1] >= sa[2:])
    elif 'wide2' in rule:
        loc = np.zeros(n - 2, bool)
        loc[1:-1] = (lv[2:-2] >= lv[1:-3]) & (lv[2:-2] >= lv[3:-1]) & (lv[2:-2] >= lv[:-4]) & (lv[2:-2] >= lv[4:])
    else:
        loc = (lv[1:-1] >= lv[:-2] + prom) & (lv[1:-1] >= lv[2:] + prom)
    peak[1:-1] = np.where(loc, lv[1:-1], 1.0)
    steep_new = 'steep' in rule
    state, slope, left, target, rel, slow = 1.0, 0.0, 0, 1.0, False, 1.0
    out = np.ones(n); st_arr = np.ones(n); sl_arr = np.ones(n)
    pk = lambda i: peak[i] if i < n - 1 else 1.0
    for o in range(n):
        now = lv[o]
        e = max(state, slow, now)
        out[o] = e; st_arr[o] = state; sl_arr[o] = slow
        if left <= 0 and slope == 0.0 and 'norest' not in rule: state = max(state, slow)
        if 'lift' in rule and rel and now > state: state = now          # a release restarts from a level above its line
        why = ''
        if left > 0:
            steep, at = -1e9, 0
            hor = window if rel else min(left, window)
            for k in range(1, hor + 1):
                v = pk(o + k); line = state + slope * k if k <= left else target
                if v > line + 1e-12 and (v - state) / k > steep: steep, at = (v - state) / k, k
            if at > 0: slope, left, target, rel = steep, at, state + steep * at, False; why = f'replan to +{at}'
        else:
            found = False
            if steep_new:
                best, at = -1e9, 0
                for k in range(1, window + 1):
                    v = pk(o + k)
                    if v > 1.0 and (v - state) / k > best: best, at = (v - state) / k, k
                if at: slope, left, target, rel = best, at, pk(o + at), False; why = f'plan to +{at}'; found = True
            else:
                for k in range(1, window + 1):
                    v = pk(o + k)
                    if v > 1.0: slope, left, target, rel = (v - state) / k, k, v, False; why = f'plan to +{k} ({v:.3f})'; found = True; break
            if not found:
                after = not rel; rel = True
                if state <= max(1.0, slow) + 1e-9: left, slope, target = 0, 0.0, state; why = 'idle'
                else:
                    to = part[o + reach_whole] if after and reach_whole > 0 and o + reach_whole < n else 1.0
                    slope, left, target = (to - state) / release, release, to; why = f'release to {to:.3f}'
        if left > 0:
            state += slope; left -= 1
            if left == 0: state = target
            if rel and state <= slow and 'nostop' not in rule: state, left, slope = slow, 0, 0.0
        slow = slow + (now - slow) * SLOW_ATT if now > slow else now + (slow - now) * SLOW_REL
        if trace and trace[0] <= o < trace[1]:
            print(f'{o:8d} level {now:.3f} peak {peak[o]:.3f} env {e:.3f} state {state:.3f} slow {slow:.3f} {why}')
    return (out, st_arr, sl_arr, lv) if states else out
