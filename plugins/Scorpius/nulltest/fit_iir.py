"""Fits a cascade of biquads (poles from (fc, Q), free numerators) to a measured path's response (an impulse response
from linear.py, less the dry part when it is a parallel path), magnitude and phase, weighted where the path is loud.
    from fit_iir import fit_path;  secs, err = fit_path(h, nsec, guesses)"""
import numpy as np
from scipy.optimize import least_squares

FS = 48000
f = np.geomspace(5, 22000, 700)
z = np.exp(-2j * np.pi * f / FS)

def den(fc, q):
    w = 2 * np.pi * fc / FS; a = np.sin(w) / (2 * q); c = np.cos(w)
    return np.array([1.0, -2 * c / (1 + a), (1 - a) / (1 + a)])

def cascade(p, nsec):
    H = np.ones_like(z); secs = []
    for s in range(nsec):
        fc, q, b0, b1, b2 = p[5 * s:5 * s + 5]
        A = den(fc, q); secs.append([b0, b1, b2, A[1], A[2]])
        H = H * (b0 + b1 * z + b2 * z ** 2) / (A[0] + A[1] * z + A[2] * z ** 2)
    return H, secs

def response(h):
    H = np.fft.rfft(h, 1 << 19); fr = np.fft.rfftfreq(1 << 19, 1 / FS)
    return np.interp(f, fr, H.real) + 1j * np.interp(f, fr, H.imag)

def fit_path(h, nsec, guesses, tries=16, floor_db=-40):
    target = response(h)
    peak = np.abs(target).max()
    w = np.maximum(np.abs(target), peak * 10 ** (floor_db / 20)) / peak
    def res(p):
        d = (cascade(p, nsec)[0] - target) / np.maximum(np.abs(target), peak * 10 ** (floor_db / 20)) * w
        return np.concatenate([d.real, d.imag])
    best = None
    for t in range(tries):
        rng = np.random.default_rng(t)
        p0 = []
        for s in range(nsec): p0 += [float(np.clip(guesses[s % len(guesses)] * np.exp(rng.normal(0, .3)), 6, 22000)), 0.5 + rng.random(), 0.1, 0, -0.1]
        try:
            s_ = least_squares(res, p0, bounds=([3, .1, -50, -50, -50] * nsec, [23000, 20, 50, 50, 50] * nsec), max_nfev=6000)
        except Exception: continue
        if best is None or s_.cost < best.cost: best = s_
    H, secs = cascade(best.x, nsec)
    mask = np.abs(target) > peak * 10 ** (floor_db / 20)
    err = np.abs(20 * np.log10(np.abs(H[mask]) / np.abs(target[mask]))).max()
    perr = np.abs(np.degrees(np.angle(H[mask] / target[mask]))).max()
    return secs, err, perr
