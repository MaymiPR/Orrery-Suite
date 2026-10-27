"""The reference's rate converters above 48 kHz (MEASUREMENTS.md 14), identified from its responses to impulses at every
phase, by alternating least squares; S (the latency it reports) places the delay so that the pair is causal.
- Integer ratios (96 kHz: 2, 192 kHz: 4): a decimating FIR d at the host rate (keep the stream's first sample and every
  R-th after) and an interpolating FIR u (zero-stuffed back to the host rate): y_c[t] = sum over q = -c (mod R) of
  d[q] u[t + S - q].
- Rational ratios (88.2 kHz: the inner samples 147 / 80 host samples apart, 176.4 kHz: 147 / 40): the same pair on the
  grid of 1 / Q host samples, v[k] = sum over j of x[j] d[P k - Q j], y[m] = sum over k of v[k] u[Q (m + S) - P k];
  the 96 kHz pair in continuous time starts the fit.
Writes the filters into Source/dsp/ConverterFits.h.
    python fit_converters.py"""
import os, time
import numpy as np
import scipy.signal as sg
import orion as o
import nullmod as N

L = 160
CASES = {96000: dict(R=2, S=48, d0=16), 192000: dict(R=4, S=80, d0=28)}
# (T: the responses' window around each impulse; span: the pair's lengths in host samples)
RATIONAL = {88200: dict(P=147, Q=80, S=46, T=(-80, 300), span=(170, 200)),
            176400: dict(P=147, Q=40, S=75, T=(-80, 520), span=(260, 300))}
ITS = 2000


def responses(fs, R):
    n = fs * 2; x = np.zeros(n, np.float32)
    pos = [fs // 4 + k * (fs // 40) + k % R for k in range(R)]
    for p in pos: x[p] = 0.5
    r = o.render(np.stack([x, x]), 'ref', fs=fs, cache=True, params=o.NONE)[0].astype(np.float64)
    return {p % R: r[p - 60:p + 200] / 0.5 for p in pos}


def identify(Y, R, S, d0):
    ts = np.arange(-60, 200)
    d = np.zeros(L); d[d0] = 1.0; u = np.zeros(L)
    def mats(fixed, solve_u):
        A, b = [], []
        for c, y in Y.items():
            M = np.zeros((len(ts), L))
            for q in range((-c) % R, L, R):
                idx = ts + S - q; ok = (idx >= 0) & (idx < L)
                if solve_u: M[np.nonzero(ok)[0], idx[ok]] += fixed[q]
                else: M[ok, q] = fixed[idx[ok]]
            A.append(M); b.append(y)
        return np.vstack(A), np.concatenate(b)
    for _ in range(40):
        A, b = mats(d, True); u, *_ = np.linalg.lstsq(A, b, rcond=None)
        A, b = mats(u, False); d, *_ = np.linalg.lstsq(A, b, rcond=None)
        k = np.sqrt(np.sum(d ** 2)); d /= k; u *= k
    e = b - A @ d
    return d, u, 10 * np.log10(np.sum(e ** 2) / np.sum(b ** 2))


def identify_rational(fs, P, Q, S, T, span, d96, u96):
    """the pair on the 1 / Q grid: impulses at each of the P phases; both least-squares steps split into small
    independent problems (each d tap belongs to one phase, gcd(P, Q) = 1; the outputs at Q (m + S) = c (mod P) involve
    only the u taps = c (mod P)), with a line search along each step"""
    gap = P * 20 + 1; pos = [fs // 4 + c * gap for c in range(P)]
    x = np.zeros(pos[-1] + 4000, np.float32)
    for p in pos: x[p] = 0.5
    r = o.render(np.stack([x, x]), 'ref', fs=fs, cache=True, params=o.NONE)[0].astype(np.float64)
    ts = np.arange(*T); Y = [r[p + T[0]:p + T[1]] / 0.5 for p in pos]
    Ld, Lu = span[0] * Q, span[1] * Q
    # the start: the 96 kHz pair in continuous time (band-limited), its delay moved to this rate's
    k96 = 96000 / fs
    def cont(h, tt, up=64):
        f = sg.resample_poly(np.concatenate([np.zeros(32), h, np.zeros(32)]), up, 1)
        return np.interp((tt + 32) * up, np.arange(len(f)), f, left=0, right=0)
    d = cont(d96, np.arange(Ld) / Q * k96) * k96
    u = cont(u96, np.arange(Lu) / Q * k96 + (48 - S * k96))
    # d step: per impulse, the design matrix's u indices
    dk = []
    for p in pos:
        ks = np.arange(-(-(Q * p) // P), (Q * p + Ld - 1) // P + 1)
        iu = Q * (p + ts[:, None] + S) - P * ks[None, :]
        dk.append((P * ks - Q * p, np.clip(iu, 0, Lu - 1), (iu >= 0) & (iu < Lu)))
    # u step: per class c, the rows (impulse, t) and their (u column, d index) pairs
    uc = []
    for c in range(P):
        R_, C_, D_, B_ = [], [], [], []
        nrow = 0
        for a, p in enumerate(pos):
            idd, iu, ok = dk[a]
            for t in np.nonzero((Q * (p + ts + S)) % P == c)[0]:
                j = np.nonzero(ok[t])[0]
                R_.append(np.full(len(j), nrow)); C_.append((iu[t, j] - c) // P); D_.append(idd[j]); B_.append(Y[a][t]); nrow += 1
        uc.append((np.concatenate(R_), np.concatenate(C_), np.concatenate(D_), np.array(B_), len(range(c, Lu, P))))
    yy = sum(np.sum(y ** 2) for y in Y)
    def resid(d, u):
        return sum(np.sum((Y[a] - np.where(ok, u[iu], 0.0) @ d[idd]) ** 2) for a, (idd, iu, ok) in enumerate(dk)) / yy
    def sweep(d, u):
        d = d.copy(); u = u.copy()
        for a, (idd, iu, ok) in enumerate(dk):
            M = np.where(ok, u[iu], 0.0)
            d[idd] = np.linalg.solve(M.T @ M + 1e-12 * np.eye(len(idd)), M.T @ Y[a])
        for c, (R_, C_, D_, b, nu) in enumerate(uc):
            A = np.zeros((len(b), nu)); np.add.at(A, (R_, C_), d[D_])
            u[c::P] = np.linalg.solve(A.T @ A + 1e-12 * np.eye(nu), A.T @ b)
        return d, u
    e = resid(d, u); t0 = time.time()
    for it in range(ITS):
        d1, u1 = sweep(d, u); e1 = resid(d1, u1)
        for a in (8.0, 4.0, 2.0):                   # the line search: further along the step, if it helps
            d2, u2 = d + a * (d1 - d), u + a * (u1 - u); e2 = resid(d2, u2)
            if e2 < e1: d1, u1, e1 = d2, u2, e2; break
        d, u, e = d1, u1, e1
        if it % 250 == 249: print(f'  {fs}: iteration {it + 1}: fit {10 * np.log10(e):.1f} dB ({time.time() - t0:.0f} s)', flush=True)
    return d, u, 10 * np.log10(e)


def simulate_rational(x, d, u, P, Q, S):
    n = len(x); K = (n * Q) // P + 2; Ld, Lu = len(d), len(u)
    v = np.zeros(K)
    for k in range(K):
        j = np.arange(max(0, -(-(P * k - Ld + 1) // Q)), min(n - 1, (P * k) // Q) + 1)
        v[k] = np.dot(x[j], d[P * k - Q * j])
    y = np.zeros(n)
    for t in range(n):
        a = Q * (t + S)
        k = np.arange(max(0, -(-(a - Lu + 1) // P)), min(K - 1, a // P) + 1)
        y[t] = np.dot(v[k], u[a - P * k])
    return y


def simulate(x, d, u, R):
    v = sg.lfilter(d, 1, x)[0::R]
    z = np.zeros(len(x)); z[0::R] = v
    return sg.lfilter(u, 1, z)


def trim(h, floor_db=-120):
    e = np.cumsum((h ** 2)[::-1])[::-1] / np.sum(h ** 2)
    return h[:int(np.nonzero(e > 10 ** (floor_db / 10))[0][-1]) + 1]


if __name__ == '__main__':
    sig = N.signals()
    lines = ['#pragma once', '#include <cmath>', '#include <vector>', '// Generated by nulltest/fit_converters.py: the reference\'s rate converters at integer ratios, identified',
             '// from its impulse responses (decimating FIR d and interpolating FIR u at the host rate).', 'namespace sc::dsp::fits', '{']
    for fs, c in CASES.items():
        R = c['R']
        d, u, res = identify(responses(fs, R), R, c['S'], c['d0'])
        nulls = []
        for k in ('drums', 'track'):
            v = sig[k]; v = v if v.ndim > 1 else np.stack([v, v])
            x = sg.resample_poly(v[:, :4 * 48000], fs // 48000, 1, axis=1).astype(np.float32)
            ref = o.render(x, 'ref', fs=fs, cache=True, params=o.NONE)[0].astype(np.float64)
            rep = o.latency('ref', fs=fs)
            y = simulate(x[0].astype(np.float64), d, u, R)
            a = ref[200:-400]; b = y[200 + rep:len(y) - 400 + rep]
            nulls.append(10 * np.log10(np.sum((a - b) ** 2) / np.sum(a ** 2)))
        g = d.sum(); d, u = trim(d / g), trim(u * g)    # (the split: the decimator at unity gain, as a plain anti-alias filter)
        print(f'{fs}: fit {res:.1f} dB, music nulls (all off) {nulls[0]:.1f} / {nulls[1]:.1f} dB, taps {len(d)} / {len(u)}')
        arr = lambda h: '{ ' + ', '.join(f'{v:.12g}' for v in h) + ' }'
        lines.append(f'inline const std::vector<double> convDown{fs // 1000} {arr(d)};')
        lines.append(f'inline const std::vector<double> convUp{fs // 1000} {arr(u)};')
        if fs == 96000: d96, u96 = d, u
    for fs, c in RATIONAL.items():
        P, Q, S = c['P'], c['Q'], c['S']
        d, u, res = identify_rational(fs, P, Q, S, c['T'], c['span'], d96, u96)
        nulls = []
        for k in ('drums', 'track'):
            v = sig[k]; v = v if v.ndim > 1 else np.stack([v, v])
            x = sg.resample_poly(v[:, :48000], P, Q, axis=1).astype(np.float32)
            ref = o.render(x, 'ref', fs=fs, cache=True, params=o.NONE)[0].astype(np.float64)
            y = simulate_rational(x[0].astype(np.float64), d, u, P, Q, S)
            a = ref[400:-400]; b = y[400:-400]
            nulls.append(10 * np.log10(np.sum((a - b) ** 2) / np.sum(a ** 2)))
        g = d.sum() / Q; d, u = trim(d / g), trim(u * g)    # (the decimator at unity gain, as above)
        print(f'{fs}: fit {res:.1f} dB, music nulls (all off) {nulls[0]:.1f} / {nulls[1]:.1f} dB, taps {len(d)} / {len(u)}')
        arr9 = lambda h: '{ ' + ', '.join(f'{v:.9g}' for v in h) + ' }'
        lines.append(f'inline const std::vector<double> convDown{fs // 1000} {arr9(d)};')
        lines.append(f'inline const std::vector<double> convUp{fs // 1000} {arr9(u)};')
    lines += ['', '// the pair at a host rate: the inner samples p / q host samples apart, the taps on the grid of 1 / q host samples',
              'struct Converters { int p, q; const std::vector<double>* down; const std::vector<double>* up; };',
              'inline const Converters* convertersAt (double fs)', '{',
              '    static const Converters all[] { { 2, 1, &convDown96, &convUp96 }, { 4, 1, &convDown192, &convUp192 },',
              '                                    { 147, 80, &convDown88, &convUp88 }, { 147, 40, &convDown176, &convUp176 } };',
              '    static constexpr double rates[] { 96000, 192000, 88200, 176400 };',
              '    for (int i = 0; i < 4; ++i) if (std::abs (fs - rates[i]) < 0.5) return all + i;',
              '    return nullptr;', '}']
    lines.append('}')
    open(os.path.join(o.PLUGIN, 'Source', 'dsp', 'ConverterFits.h'), 'w').write('\n'.join(lines) + '\n')
