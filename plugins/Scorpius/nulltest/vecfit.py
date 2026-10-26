"""z-domain vector fitting (Gustavsen's pole relocation with the basis 1 / (1 - p z^-1)): a measured frequency
response as d + sum r_k / (1 - p_k z^-1); returned as parallel sections (b0, b1, a1, a2),
H(z) = d + sum (b0 + b1 z^-1) / (1 + a1 z^-1 + a2 z^-2) (a first-order section has a2 = 0). Used for linear paths
that are not textbook filters (the drive's).
    from vecfit import fit_sections, parallel_response
    d, secs = fit_sections(w, H, order, weight)"""
import numpy as np


def _basis(zi, poles):
    return 1.0 / (1.0 - poles[None, :] * zi[:, None])


def vecfit(w, H, order, iters=40, weight=None, init=None):
    zi = np.exp(-1j * w)
    zi2 = np.concatenate([zi, np.conj(zi)]); H2 = np.concatenate([H, np.conj(H)])
    W = np.ones(len(H2)) if weight is None else np.concatenate([weight, weight])
    if init is None:
        npair = order // 2
        ang = np.geomspace(max(w[0], 1e-4), w[-1] * 0.9, npair)
        p = 0.99 * np.exp(1j * ang) * np.exp(-ang / 50)
        poles = np.concatenate([p, np.conj(p)] + ([np.array([0.5])] if order % 2 else []))
    else:
        poles = np.asarray(init, complex)
    for _ in range(iters):
        B = _basis(zi2, poles)
        A = np.hstack([B, np.ones((len(zi2), 1)), -H2[:, None] * B]) * W[:, None]
        x = np.linalg.lstsq(A, H2 * W, rcond=None)[0]
        n = len(poles); c = x[n + 1:]
        a = 1 + c.sum(); e = c * poles
        poles = np.linalg.eigvals(np.diag(poles) - np.outer(np.ones(n), e) / a)
        poles = np.where(np.abs(poles) > 1, 1 / np.conj(poles), poles)      # reflect into the unit circle
    B = _basis(zi2, poles)
    A = np.hstack([B, np.ones((len(zi2), 1))]) * W[:, None]
    x = np.linalg.lstsq(A, H2 * W, rcond=None)[0]
    return x[len(poles)].real, poles, x[:len(poles)]


def to_sections(d, poles, res):
    """pairs conjugate poles into real second-order sections; real poles become first-order sections"""
    secs, used = [], np.zeros(len(poles), bool)
    for i, p in enumerate(poles):
        if used[i]: continue
        used[i] = True
        if abs(p.imag) < 1e-9 * max(1, abs(p)):
            secs.append((res[i].real, 0.0, -p.real, 0.0)); continue
        j = min((k for k in range(len(poles)) if not used[k]), key=lambda k: abs(poles[k] - np.conj(p)))
        used[j] = True
        q = p if p.imag > 0 else poles[j]; r = res[i] if p.imag > 0 else res[j]
        secs.append((2 * r.real, -2 * (r * np.conj(q)).real, -2 * q.real, abs(q) ** 2))
    return d, secs


def fit_sections(w, H, order, weight=None, iters=40):
    d, poles, res = vecfit(w, H, order, iters, weight)
    return to_sections(d, poles, res)


def parallel_response(d, secs, w):
    zi = np.exp(-1j * w); Hm = d * np.ones_like(zi)
    for b0, b1, a1, a2 in secs: Hm = Hm + (b0 + b1 * zi) / (1 + a1 * zi + a2 * zi ** 2)
    return Hm
