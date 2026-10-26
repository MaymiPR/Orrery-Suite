"""Steiglitz-McBride: an IIR filter (nb zeros, na poles) fitted to an impulse response in the time domain; returned as
second-order sections (scipy's sos layout). Used for the linear paths that are not textbook filters."""
import numpy as np
from scipy.signal import lfilter, tf2sos, freqz

def stmcb(h, nb, na, iters=12):
    n = len(h); x = np.zeros(n); x[0] = 1.0
    # Prony start
    H = np.zeros((n, na))
    for k in range(1, na + 1): H[k:, k - 1] = -h[:n - k]
    a = np.concatenate([[1.0], np.linalg.lstsq(H[nb + 1:], h[nb + 1:], rcond=None)[0]])
    for _ in range(iters):
        xf = lfilter([1.0], a, x); hf = lfilter([1.0], a, h)
        cols = []
        for k in range(nb + 1): cols.append(np.concatenate([np.zeros(k), xf[:n - k]]))
        for k in range(1, na + 1): cols.append(-np.concatenate([np.zeros(k), hf[:n - k]]))
        A = np.stack(cols, 1)
        sol = np.linalg.lstsq(A, h, rcond=None)[0]
        b = sol[:nb + 1]; a = np.concatenate([[1.0], sol[nb + 1:]])
    return b, a

def check(h, b, a, fs=48000):
    y = lfilter(b, a, np.eye(1, len(h), 0)[0])
    err = 10 * np.log10(np.sum((y - h) ** 2) / np.sum(h ** 2))
    stable = np.all(np.abs(np.roots(a)) < 1)
    return err, stable
