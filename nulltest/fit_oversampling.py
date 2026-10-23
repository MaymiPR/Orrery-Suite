"""Designs our oversampling filters (Source/dsp/OsFilters.h).

The reference's bypassed path gets 22.49 / 27.71 / 29.66 samples longer at 2x / 4x / 8x, so its filters are short. Ours
are linear-phase FIR stages of 45 / 21 / 15 taps (22 / 27 / 28.75 samples; the rest is the fixed delay, which sits
inside the oversampled part, fraction included, as Source/dsp/Engine.cpp does).

The first stage (one design for 2x, one shared by 4x and 8x, as the reference's responses suggest) is fitted so that
the whole simulated path matches the reference's measured magnitude (the phase then agrees to a few degrees below 20 kHz). That path is up-sampling stages, then the
fractional delay at the high rate, then down-sampling stages. Fitting the whole path matters: the down-sampler folds
the image back, and how it adds depends on the delay in between. Uses the cached reference renders of
measure_oversampling.py (salts 'os2', 'os4', 'os8')."""
import os
import numpy as np
from scipy.optimize import least_squares
from scipy.signal import lfilter
import tgp

FS = tgp.FS
PRE = 2 * FS
N = 4096
CORE = 151.27917                                  # the fixed delay at 48 kHz (Tuning.h coreDelay)
EXTRA = {2: 22.488, 4: 27.711, 8: 29.655}         # Engine.cpp oversamplingExtra


def halfband_ls(ntaps, rate, target_f, target_mag, weight):
    """least-squares half-band: H(w) = 0.5 + sum_k 2 c_k cos((2k - 1) w)"""
    pairs = (ntaps + 1) // 4
    w = 2 * np.pi * target_f / rate
    A = np.stack([2 * np.cos((2 * k - 1) * w) for k in range(1, pairs + 1)], 1)
    c = np.linalg.lstsq(A * weight[:, None], (target_mag - 0.5) * weight, rcond=None)[0]
    h = np.zeros(ntaps); mid = ntaps // 2
    h[mid] = 0.5
    for k in range(1, pairs + 1): h[mid + 2 * k - 1] = h[mid - (2 * k - 1)] = c[k - 1]
    return h


def simulate(h1, s2, s3, k):
    """impulse response (base rate, N samples) of: up-sampling stages, the fixed delay (Thiran fraction), down-sampling"""
    stages = [h1, s2, s3][:{2: 1, 4: 2, 8: 3}[k]]
    x = np.zeros(N); x[0] = 1.0
    for h in stages:
        z = np.zeros(2 * len(x)); z[0::2] = x
        x = 2 * np.convolve(z, h)[:len(z)]
    lat = sum((len(h) - 1) / (2 << i) for i, h in enumerate(stages))
    d = (CORE + EXTRA[k] - lat) * k
    whole = int(np.floor(d)); frac = d - whole
    if frac < 0.05 and whole > 0: frac += 1; whole -= 1
    a = (1 - frac) / (1 + frac)
    x = lfilter([a, 1], [1, a], np.concatenate([np.zeros(whole), x])[:len(x)])
    for h in stages[::-1]:
        x = np.convolve(x, h)[:len(x)][0::2]
    return x[:N]


if __name__ == '__main__':
    imp = np.zeros(PRE + 32768, np.float32); imp[PRE] = 1e-3       # the same input as measure_oversampling.py (its cached renders)
    f = np.fft.rfftfreq(N, 1 / FS)
    sel = (f > 20) & (f < 23900)
    jobs = [dict(x=imp, norm=tgp.NONE, raw=True, salt=f'os{k}') for k in (2, 4, 8)]
    if not all(os.path.exists(os.path.join(tgp.CACHE, tgp._key(j, 'ref', FS, 512) + '.npy')) for j in jobs):
        raise SystemExit('run measure_oversampling.py first (the reference has to be switched to each ratio)')
    refs = {k: np.fft.rfft(y[0, PRE:PRE + N] / 1e-3) for k, y in zip((2, 4, 8), tgp.render_many(jobs, cache=True))}
    # stages 2 and 3: flat to 24 kHz, as much rejection as their length allows above (rate - 24 kHz)
    def plain(ntaps, rate):
        ff = np.linspace(0, 24000, 400)
        return halfband_ls(ntaps, rate, ff, np.ones_like(ff), np.ones_like(ff))
    s2, s3 = plain(21, 4 * FS), plain(15, 8 * FS)
    mid = 22
    def unpack(c):
        h = np.zeros(45); h[mid] = c[0]; h[mid + 1:] = c[1:]; h[:mid] = c[1:][::-1]; return h
    weight = np.sqrt(np.where(f[sel] > 15000, 1.0, 0.3))
    stopf = np.linspace(28000, 48000, 200)
    def stop(h):     # keep images of < 20 kHz small (they become aliasing of the character's harmonics)
        w = 2 * np.pi * stopf / (2 * FS)
        return h[mid] + sum(2 * h[mid + i] * np.cos(i * w) for i in range(1, mid + 1))
    stage1 = {}
    for ks in ((2,), (4, 8)):
        def res(c):
            h = unpack(c); out = []
            for k in ks:
                e = np.fft.rfft(simulate(h, s2, s3, k))[sel] / refs[k][sel]
                out += [20 * np.log10(np.abs(e)) * weight]            # magnitude (the phase follows from the structure)
            return np.concatenate(out + [0.02 * stop(h)])
        # start: a half-band whose square (both stages) has the measured magnitude
        mag = np.abs(refs[ks[0]][sel])
        h0 = halfband_ls(45, 2 * FS, f[sel], np.sqrt(mag), weight ** 2)
        c = least_squares(res, np.concatenate([[h0[mid]], h0[mid + 1:]])).x
        h = unpack(c)
        for k in ks:
            e = np.fft.rfft(simulate(h, s2, s3, k)) / refs[k]
            print(f'{k}x: max error below 23.9 kHz {np.max(np.abs(20 * np.log10(np.abs(e[sel])))):.3f} dB, {np.max(np.abs(np.degrees(np.angle(e[sel])))):.2f} deg;',
                  'at 20 / 22 / 23 kHz (dB):', [round(float(20 * np.log10(abs(np.interp(q, f, e.real) + 1j * np.interp(q, f, e.imag)))), 3) for q in (20000, 22000, 23000)])
        for k in ks: stage1[k] = h
    def arr(name, h):
        out = [f'inline constexpr int {name}Taps = {len(h)};', f'inline constexpr double {name}[{len(h)}] {{']
        for i in range(0, len(h), 5): out.append('    ' + ', '.join(f'{v:.15g}' for v in h[i:i + 5]) + ',')
        return out + ['};']
    lines = ['#pragma once', '// Generated by nulltest/fit_oversampling.py: FIR stages of the oversampler (Oversampler.h). Stage 1 is fitted so',
             "// that the whole oversampled path matches the reference's (one design for 2x, one for 4x and 8x).", 'namespace ap::dsp', '{']
    lines += arr('osStage1x2', stage1[2]) + arr('osStage1x4', stage1[4]) + arr('osStage2', s2) + arr('osStage3', s3) + ['}', '']
    open(os.path.join(tgp.ROOT, 'Source', 'dsp', 'OsFilters.h'), 'w').write('\n'.join(lines))
