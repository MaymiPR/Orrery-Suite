"""Master limiter (EQ and character bypassed):
  static    1 kHz sine staircase, output peak and fundamental vs input, for several Limiter Input Gain settings
  pregain   small-signal gain vs the Limiter Input Gain setting (the knob's dB law)
  ceiling   sample-peak and 4x-oversampled (true) peak of the output for loud noise and loud 9 / 15 kHz sines
  envelope  loud 60 Hz bursts of several lengths with a quiet 15 kHz pilot: the limiter's gain vs time (look-ahead,
            attack shape, release law and whether it depends on how long the overload lasted)
  step      DC steps (loud / the same x 1/1000): output ratio = the limiter's gain at sample resolution (look-ahead ramp,
            fast and slow release, how the slow part charges)
  stereo    loud L only, pilot on R (linking)
Writes measurements/limiter/limiter.json and plots."""
import json, os
import numpy as np
from scipy.signal import resample_poly
import tgp, plotstyle as ps

OUT = os.path.join(tgp.ROOT, 'measurements', 'limiter'); os.makedirs(OUT, exist_ok=True)
FS = tgp.FS
PRE = 2 * FS
STEP = 0.6
LEVELS = list(range(-30, 13, 2))
PREGAINS = [0, 2.5, 5, 8, 12]
L = lambda: {**tgp.LIM_ONLY}


def sig(*parts):
    return np.concatenate([np.zeros(PRE)] + list(parts)).astype(np.float32)


def sine(f, secs, db, t0=0.0):
    t = t0 + np.arange(int(secs * FS)) / FS
    return 10 ** (db / 20) * np.sin(2 * np.pi * f * t)


def lockin(y, f, a, b):
    w = np.hanning(b - a); t = np.arange(a, b) / FS
    return abs(np.sum(y[a:b] * w * np.exp(-2j * np.pi * f * t)) / np.sum(w) * 2)


def staircase(f=1000):
    return sig(np.concatenate([sine(f, STEP, l, i * STEP) for i, l in enumerate(LEVELS)]))


def true_peak(y):
    return 20 * np.log10(np.max(np.abs(resample_poly(y, 4, 1))))


# --- envelope: pilot ---------------------------------------------------------------------------------------------
PILOT = 15000; PDB = -40
BURSTS = [0.005, 0.05, 0.3, 1.0, 3.0]          # overload lengths (s)
LOUD = 6                                        # 60 Hz at +6 dBFS (~ 11 dB over the ceiling with the default pre-gain)


def burst_sig(dur, loud=LOUD):
    n = int((1.0 + max(BURSTS) + 4.0) * FS)
    x = np.zeros(n); a = int(1.0 * FS); b = a + int(dur * FS)
    x[a:b] = sine(60, (b - a) / FS, loud)
    p = sine(PILOT, n / FS, PDB)
    return sig(x + p), sig(p)


RAW = 190                                       # raw delay of this configuration (core 152 + look-ahead 38)
STEPS = [(2.0, 0.05, 1.0), (2.0, 0.05, 0.02), (2.0, 0.05, 0.005), (1.2, 0.05, 1.0)]   # (loud, quiet, loud seconds)


def step_sig(hi, lo, dur):
    x = np.full(PRE + int((1 + dur + 3) * FS), lo); a = PRE + FS; x[a:a + int(dur * FS)] = hi
    return x


def pilot_gain(y, ref):
    """Gain on the pilot vs time (1 ms resolution): complex demodulation at the pilot frequency, re the pilot alone."""
    t = np.arange(len(y)) / FS
    lo = np.exp(-2j * np.pi * PILOT * t)
    k = np.hanning(97); k /= k.sum()                      # ~2 ms smoothing: rejects 2 f and the 60 Hz harmonics
    zy = np.convolve(y * lo, k, 'same'); zr = np.convolve(ref * lo, k, 'same')
    return 20 * np.log10(np.abs(zy) / np.abs(zr))


if __name__ == '__main__':
    jobs, tags = [], []
    for pg in PREGAINS:
        jobs.append(dict(x=staircase(), norm=L(), params={'Limiter Input Gain': pg}, raw=True)); tags.append(('static', pg))
    imp = np.zeros(PRE + 8192, np.float32); imp[PRE] = 10 ** (-60 / 20)
    jobs.append(dict(x=imp, norm=tgp.NONE, raw=True)); tags.append(('imp_none', 0))
    for pg in (0, 1, 2, 3, 4, 5, 6, 8, 10, 12):
        jobs.append(dict(x=imp, norm=L(), params={'Limiter Input Gain': pg}, raw=True)); tags.append(('imp', pg))
    loudsig = {'noise +6 dBFS RMS-12': sig(tgp.noise(4, -6, seed=7)), 'pink +0 crest': sig(tgp.noise(4, -10, seed=8, pink=True)),
               'sine 9 kHz +6': sig(sine(9000, 4, 6)), 'sine 15 kHz +6': sig(sine(15000, 4, 6)), 'sine 11025 Hz +6 (pi/2 phase steps)': sig(sine(12000, 4, 6))}
    for k, x in loudsig.items():
        jobs.append(dict(x=x, norm=L(), raw=True)); tags.append(('ceil', k))
        jobs.append(dict(x=x, norm=L(), params={'Limiter Input Gain': 12}, raw=True)); tags.append(('ceil12', k))
    for d in BURSTS:
        x, p = burst_sig(d)
        jobs.append(dict(x=x, norm=L(), raw=True)); tags.append(('burst', d))
    jobs.append(dict(x=burst_sig(1.0)[1], norm=L(), raw=True)); tags.append(('pilot', 0))
    for d in (0.05, 1.0):
        jobs.append(dict(x=burst_sig(d, loud=0)[0], norm=L(), raw=True)); tags.append(('burst0', d))
    xs = burst_sig(1.0)[0]; ps_ = burst_sig(1.0)[1]
    jobs.append(dict(x=np.stack([xs, ps_]), norm=L(), raw=True)); tags.append(('stereo', 0))
    for st in STEPS:
        jobs.append(dict(x=step_sig(*st), norm=L(), raw=True)); tags.append(('step', st))
        jobs.append(dict(x=step_sig(*st) / 1000, norm=L(), raw=True)); tags.append(('stepq', st))
    ys = tgp.render_many(jobs, cache=True)
    Y = dict(zip([tuple(t) for t in tags], ys))

    res = {}
    # static
    stat = {}
    for pg in PREGAINS:
        y = Y[('static', pg)][0]; rows = []
        for i, l in enumerate(LEVELS):
            a = PRE + int((i + 0.5) * STEP * FS); b = PRE + int((i + 1) * STEP * FS)
            rows.append(dict(in_dBFS=l, fund_out_dBFS=round(20 * np.log10(lockin(y, 1000, a, b)), 3),
                             peak_out_dBFS=round(float(20 * np.log10(np.max(np.abs(y[a:b])))), 3)))
        stat[str(pg)] = rows
    res['static_1k'] = stat
    # pre-gain law and the stage's small-signal response
    H0 = np.fft.rfft(Y[('imp_none', 0)][0, PRE:]); f = np.fft.rfftfreq(len(H0) * 2 - 2, 1 / FS)
    law = {}
    for pg in (0, 1, 2, 3, 4, 5, 6, 8, 10, 12):
        h = Y[('imp', pg)][0, PRE:]
        H = np.fft.rfft(h) / H0
        g = 20 * np.log10(np.abs(H))
        law[str(pg)] = dict(gain_1k_dB=round(float(np.interp(1000, f, g)), 4), ripple_20_20k_dB=[round(float(g[(f > 20) & (f < 20000)].min()), 3), round(float(g[(f > 20) & (f < 20000)].max()), 3)],
                            delay_re_bypassed=int(np.argmax(np.abs(h)) - np.argmax(np.abs(Y[('imp_none', 0)][0, PRE:]))))
    res['pregain_small_signal'] = law
    # ceiling
    ceil = {}
    for k in loudsig:
        for tag in ('ceil', 'ceil12'):
            y = Y[(tag, k)][0, PRE + FS:]
            ceil[f'{k} | pregain {"5" if tag == "ceil" else "12"}'] = dict(sample_peak_dBFS=round(float(20 * np.log10(np.max(np.abs(y)))), 3),
                                                                     true_peak_dBFS=round(float(true_peak(y)), 3),
                                                                     rms_dBFS=round(tgp.rms_db(y), 2))
    res['ceiling'] = ceil
    # envelopes
    ref = Y[('pilot', 0)][0]
    env = {}
    fg, ax = ps.fig('Limiter gain vs time: 60 Hz bursts at +6 dBFS (pre-gain 5 dB), 15 kHz pilot', 'ms after burst start', 'dB', size=(10, 4.5))
    for d in BURSTS:
        g = pilot_gain(Y[('burst', d)][0], ref)
        a = PRE + FS + RAW; b = a + int(d * FS)
        dec = g[a - 50 * 48: b + 3000 * 48: 48]
        env[str(d)] = dict(min_dB=round(float(np.min(g[a:b + 48 * 20])), 3),
                           pre_onset_ms=[round(float(g[a + int(ms * 48)]), 3) for ms in (-2, -1, -0.75, -0.5, -0.25, 0)],
                           after_end_ms={str(ms): round(float(g[b + int(ms * 48)]), 3) for ms in (1, 5, 10, 20, 50, 100, 200, 500, 1000, 2000)},
                           curve_ms_dB=dec.round(3).tolist())
        ax.plot(np.arange(len(dec)) - 50, dec, label=f'{d * 1000:.0f} ms', lw=1.3)
    ax.set_xlim(-50, 4000)
    ps.save(fg, os.path.join(OUT, 'release_vs_burst_length.png'))
    for d in (0.05, 1.0):
        g = pilot_gain(Y[('burst0', d)][0], ref); a = PRE + FS + RAW; b = a + int(d * FS)
        env[f'{d} @0dBFS'] = dict(min_dB=round(float(np.min(g[a:b + 960])), 3),
                                  after_end_ms={str(ms): round(float(g[b + int(ms * 48)]), 3) for ms in (1, 5, 10, 20, 50, 100, 200, 500, 1000, 2000)})
    res['envelope_pilot'] = env
    steps = {}
    fg, ax = ps.fig('Limiter gain vs time, DC steps (pre-gain 5 dB): release', 'ms after the last loud sample', 'dB', size=(10, 4.5))
    for st in STEPS:
        g = 20 * np.log10(np.abs(Y[('step', st)][0]) / np.maximum(np.abs(Y[('stepq', st)][0]) * 1000, 1e-12))
        a = PRE + FS + RAW - 1; b = a + int(st[2] * FS)        # first loud sample at the output (measured: 189 samples)
        steps[f'{st[0]} for {st[2] * 1000:.0f} ms'] = dict(
            onset_samples={str(k): round(float(g[a + k]), 3) for k in range(-24, 13)},
            hold_dB=round(float(g[b - 1]), 3),
            after_end_samples={str(k): round(float(g[b + k]), 3) for k in (0, 1, 2, 4, 8, 12, 16, 24, 32)},
            after_end_ms={str(ms): round(float(g[b + int(ms * 48)]), 3) for ms in (1, 5, 10, 20, 50, 100, 200, 300, 500, 700, 1000, 1500, 2000, 2500)})
        dec = g[b - 48 * 10: b + 48 * 2500: 48]
        ax.plot(np.arange(len(dec)) - 10, dec, label=f'{20 * np.log10(st[0] * 10 ** 0.25):+.1f} dB peak for {st[2] * 1000:.0f} ms', lw=1.3)
    ps.save(fg, os.path.join(OUT, 'release_steps.png'))
    fg, ax = ps.fig('Limiter gain at the onset of a DC step (sample resolution)', 'samples re the first loud output sample', 'dB')
    for st in STEPS[:1] + STEPS[3:]:
        k = steps[f'{st[0]} for {st[2] * 1000:.0f} ms']['onset_samples']
        ax.plot([int(q) for q in k], list(k.values()), marker='o', ms=3, label=f'step to {20 * np.log10(st[0] * 10 ** 0.25):+.1f} dB')
    ps.save(fg, os.path.join(OUT, 'onset_steps.png'))
    # release law: (1/g - 1) decays exponentially; fit tau on the long step
    k = steps['2.0 for 1000 ms']['after_end_ms']
    e = {int(m): 10 ** (-v / 20) - 1 for m, v in k.items() if int(m) >= 50 and int(m) <= 2000}
    m0 = min(e); taus = {str(m): round((m - m0) / 1000 / np.log(e[m0] / e[m]), 4) for m in e if m != m0}
    steps['release: tau of (1/gain - 1) from 50 ms on (s)'] = taus
    res['steps'] = steps
    y = Y[('stereo', 0)]
    gl = pilot_gain(y[1], ref)
    res['stereo'] = dict(R_pilot_gain_during_L_burst_dB=round(float(np.median(gl[PRE + FS + 4800: PRE + 2 * FS])), 3))
    json.dump(res, open(os.path.join(OUT, 'limiter.json'), 'w'), indent=1)

    fg, ax = ps.fig('Limiter, 1 kHz sine: output peak vs input (static)', 'input dBFS', 'output peak dBFS')
    for pg in PREGAINS:
        ax.plot(LEVELS, [r['peak_out_dBFS'] for r in stat[str(pg)]], label=f'input gain {pg} dB')
    ps.save(fg, os.path.join(OUT, 'static_1k.png'))
    for k in ('pregain_small_signal', 'ceiling', 'stereo'): print(k, json.dumps(res[k], indent=0))
    for k, v in env.items(): print(k, {kk: vv for kk, vv in v.items() if kk != 'curve_ms_dB'} if isinstance(v, dict) else v)
    for k, v in steps.items(): print(k, v)
    for pg in PREGAINS: print('static', pg, [(r['in_dBFS'], r['peak_out_dBFS'], r['fund_out_dBFS']) for r in stat[str(pg)]][::2])
