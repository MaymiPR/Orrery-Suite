"""Character section (EQ and limiter bypassed), dynamic behaviour at amount 100 % unless noted:
  bands     a loud masker tone plus a comb of quiet probe tones: which probes lose gain tells the band edges
  envelope  tone bursts (-40 -> step -> -40 dBFS): attack and release of the gain, per band
  detector  one sine vs two tones of the same RMS (3 dB more peak): peak or RMS detection
  stereo    loud tone on L only, quiet probe on R (linking); pure side vs pure mid (M/S processing)
Writes measurements/character/dyn.json."""
import json, os
import numpy as np
from scipy.signal import hilbert
import tgp

OUT = os.path.join(tgp.ROOT, 'measurements', 'character'); os.makedirs(OUT, exist_ok=True)
FS = tgp.FS
PRE = 2 * FS                     # skip the start-up delay ramp
CORE = 152                       # raw delay of the path with the limiter bypassed
AM = 0.5


def lockin(y, f, a, b):
    w = np.hanning(b - a); t = np.arange(a, b) / FS
    z = np.sum(y[a:b] * w * np.exp(-2j * np.pi * f * t)) / np.sum(w) * 2
    return abs(z)


def sig(*parts):
    return np.concatenate([np.zeros(PRE)] + list(parts)).astype(np.float32)


def sine(f, secs, db, t0=0.0, phase=0.0):
    t = t0 + np.arange(int(secs * FS)) / FS
    return 10 ** (db / 20) * np.sin(2 * np.pi * f * t + phase)


# --- bands ------------------------------------------------------------------------------------------------------
PROBES = []
for f in np.geomspace(25, 20000, 44):
    f = int(round(f))
    while not (7 <= f % 50 <= 43): f += 1
    PROBES.append(f)
rng = np.random.default_rng(5)
PH = rng.uniform(0, 2 * np.pi, len(PROBES))
MASKERS = [50, 150, 400, 1000, 2500, 6000, 12000]
SECS = 3.0


def comb(db=-60):
    return sum(sine(f, SECS, db, phase=p) for f, p in zip(PROBES, PH))


def bands_jobs():
    jobs = [dict(x=sig(comb()), norm={**tgp.CHAR_ONLY, 'Character': AM}, raw=True)]
    for m in MASKERS:
        jobs.append(dict(x=sig(comb() + sine(m, SECS, -3)), norm={**tgp.CHAR_ONLY, 'Character': AM}, raw=True))
    return jobs


def bands_eval(ys):
    a, b = PRE + int(1.0 * FS), PRE + int(SECS * FS)
    ref = np.array([lockin(ys[0][0], f, a, b) for f in PROBES])
    out = {}
    for m, y in zip(MASKERS, ys[1:]):
        g = np.array([lockin(y[0], f, a, b) for f in PROBES]) / ref
        out[str(m)] = (20 * np.log10(g)).round(3).tolist()
    return dict(probes_hz=PROBES, masker_dBFS=-3, probe_dBFS=-60, probe_gain_re_alone_dB=out)


# --- envelope ---------------------------------------------------------------------------------------------------
ENV_F = [100, 1000, 8000]
ENV_STEPS = [-20, -6, 0]
BASE = -40


def burst(f, hi):
    a, b, c = 2.0, 1.5, 3.0
    return sig(np.concatenate([sine(f, a, BASE), sine(f, b, hi, a), sine(f, c, BASE, a + b)])), (a, b, c)


def env_jobs():
    return [dict(x=burst(f, hi)[0], norm={**tgp.CHAR_ONLY, 'Character': AM}, raw=True) for f in ENV_F for hi in ENV_STEPS]


def gain_env(x, y, f):
    """Gain of the fundamental vs time: band-pass both around f, analytic envelopes, delay aligned."""
    n = len(x); F = np.fft.rfftfreq(n, 1 / FS)
    m = np.exp(-0.5 * (np.log(np.maximum(F, 1) / f) / 0.25) ** 2)      # smooth (Gaussian in log f) band-pass, no ringing
    bp = lambda s: np.fft.irfft(np.fft.rfft(s) * m, n)
    ex = np.abs(hilbert(bp(x))); ey = np.abs(hilbert(bp(y)))
    ey = np.concatenate([ey[CORE:], np.zeros(CORE)])
    return 20 * np.log10(np.maximum(ey, 1e-9) / np.maximum(ex, 1e-9))


def time_to(g, t0, target, start, end):
    """Seconds after t0 until g first comes within 'fraction' of the way from start to end (g is per sample)."""
    i0 = int(t0 * FS)
    seg = g[i0:i0 + int(2.5 * FS)]
    lvl = start + target * (end - start)
    idx = np.where((seg - lvl) * np.sign(end - start) >= 0)[0]
    return None if len(idx) == 0 else round(idx[0] / FS * 1000, 2)


def env_eval(jobs, ys):
    out = {}
    i = 0
    for f in ENV_F:
        for hi in ENV_STEPS:
            x, (a, b, c) = burst(f, hi)
            g = gain_env(x, ys[i][0], f); i += 1
            t = PRE / FS
            g0 = float(np.median(g[int((t + a - 0.5) * FS):int((t + a - 0.05) * FS)]))
            g1 = float(np.median(g[int((t + a + b - 0.3) * FS):int((t + a + b - 0.05) * FS)]))
            g2 = float(np.median(g[int((t + a + b + c - 0.5) * FS):int((t + a + b + c - 0.05) * FS)]))
            dec = slice(int(t * FS), len(g), 48)              # 1 ms resolution for the stored curve
            out[f'{f}|{hi}'] = dict(base_dB=round(g0, 3), loud_dB=round(g1, 3), after_dB=round(g2, 3),
                                    attack_ms={p: time_to(g, t + a, p / 100, g0, g1) for p in (10, 50, 63, 90)},
                                    release_ms={p: time_to(g, t + a + b, p / 100, g1, g0) for p in (10, 50, 63, 90)},
                                    curve_ms_dB=g[dec].round(3).tolist())
    return out


# --- detector ---------------------------------------------------------------------------------------------------
DET = [(1000, 1100), (100, 110), (8000, 8800)]
DET_L = [-30, -20, -10, -4]


def det_jobs():
    jobs = []
    for f1, f2 in DET:
        for l in DET_L:
            jobs.append(dict(x=sig(sine(f1, 3, l)), norm={**tgp.CHAR_ONLY, 'Character': AM}, raw=True))
            jobs.append(dict(x=sig(sine(f1, 3, l - 3.0103) + sine(f2, 3, l - 3.0103)), norm={**tgp.CHAR_ONLY, 'Character': AM}, raw=True))
    return jobs


def det_eval(ys):
    a, b = PRE + int(1.5 * FS), PRE + 3 * FS
    out = {}; i = 0
    for f1, f2 in DET:
        for l in DET_L:
            g1 = 20 * np.log10(lockin(ys[i][0], f1, a, b)) - l
            g2 = 20 * np.log10(lockin(ys[i + 1][0], f1, a, b)) - (l - 3.0103)
            out[f'{f1}|{l}'] = dict(sine_gain_dB=round(g1, 3), two_tone_same_rms_gain_dB=round(g2, 3)); i += 2
    return out


# --- stereo -----------------------------------------------------------------------------------------------------
def st_jobs():
    n = AM
    probe = sine(1200, 3, -50); loud = sine(1000, 3, -6)
    s = lambda l, r: np.stack([sig(l), sig(r)])
    return [dict(x=s(0 * probe, probe), norm={**tgp.CHAR_ONLY, 'Character': n}, raw=True),
            dict(x=s(loud, probe), norm={**tgp.CHAR_ONLY, 'Character': n}, raw=True),
            dict(x=s(sine(1000, 3, -10), sine(1000, 3, -10)), norm={**tgp.CHAR_ONLY, 'Character': n}, raw=True),
            dict(x=s(sine(1000, 3, -10), -sine(1000, 3, -10)), norm={**tgp.CHAR_ONLY, 'Character': n}, raw=True),
            dict(x=s(sine(1000, 3, -10), 0 * probe), norm={**tgp.CHAR_ONLY, 'Character': n}, raw=True),
            dict(x=s(sine(1000, 3, -40), -sine(1000, 3, -40)), norm={**tgp.CHAR_ONLY, 'Character': n}, raw=True),
            dict(x=s(sine(1000, 3, -40), sine(1000, 3, -40)), norm={**tgp.CHAR_ONLY, 'Character': n}, raw=True)]


def st_eval(ys):
    a, b = PRE + int(1.5 * FS), PRE + 3 * FS
    pr = lambda y: 20 * np.log10(lockin(y[1], 1200, a, b))
    g = lambda y, ch, l: round(20 * np.log10(lockin(y[ch], 1000, a, b)) - l, 3)
    return {'R probe gain change with loud L (dB, linking)': round(pr(ys[1]) - pr(ys[0]), 3),
            'L leak into R with L only (dBFS at 1 kHz)': round(20 * np.log10(lockin(ys[4][1], 1000, a, b) + 1e-12), 2),
            '-10 dBFS mid: gain L, R': [g(ys[2], 0, -10), g(ys[2], 1, -10)],
            '-10 dBFS side: gain L, R': [g(ys[3], 0, -10), g(ys[3], 1, -10)],
            '-10 dBFS L only: gain L': g(ys[4], 0, -10),
            '-40 dBFS side: gain L, R': [g(ys[5], 0, -40), g(ys[5], 1, -40)],
            '-40 dBFS mid: gain L, R': [g(ys[6], 0, -40), g(ys[6], 1, -40)]}


if __name__ == '__main__':
    bj, ej, dj, sj = bands_jobs(), env_jobs(), det_jobs(), st_jobs()
    ys = tgp.render_many(bj + ej + dj + sj, cache=True)
    o = 0
    res = {'bands': bands_eval(ys[o:o + len(bj)])}; o += len(bj)
    res['envelope'] = env_eval(ej, ys[o:o + len(ej)]); o += len(ej)
    res['detector'] = det_eval(ys[o:o + len(dj)]); o += len(dj)
    res['stereo'] = st_eval(ys[o:o + len(sj)])
    json.dump(res, open(os.path.join(OUT, 'dyn.json'), 'w'), indent=1)
    print('bands: probe gain (dB) re probes alone, masker -3 dBFS')
    print('  probe  ' + ' '.join(f'{m:>6d}' for m in MASKERS))
    for i, f in enumerate(PROBES):
        print(f'  {f:5d}  ' + ' '.join(f'{res["bands"]["probe_gain_re_alone_dB"][str(m)][i]:6.2f}' for m in MASKERS))
    for k, v in res['envelope'].items():
        print('env', k, {kk: vv for kk, vv in v.items() if kk != 'curve_ms_dB'})
    for k, v in res['detector'].items(): print('det', k, v)
    for k, v in res['stereo'].items(): print('st', k, v)
