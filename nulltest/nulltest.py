"""Null tests: the same signals and settings through the reference (Cradle The God Particle) and through The
Antiparticle; the residual (reference - ours) in dB re the reference's output. Both renders are latency-compensated by
each plug-in's reported latency, so a latency mismatch shows up as residual too.

    python nulltest.py                 # every case, our processor via ApRender (fast)
    python nulltest.py --vst3          # our built VST3 through RefRender (the shipped binary)
    python nulltest.py --only lim      # cases whose name contains 'lim'
Writes nulltest/results.json and prints a table; NULLTESTS.md reports it."""
import argparse, json, os
import numpy as np
import tgp

FS = tgp.FS
PRE = 2 * FS                    # the reference's start-up delay ramp (MEASUREMENTS.md); excluded from the residual
SECS = 6


def sig_noise(db=-18, pink=True, seed=11):
    return tgp.noise(SECS, db, seed=seed, pink=pink)


def sig_sweep(db=-12, f0=20, f1=20000):
    t = np.arange(int(SECS * FS)) / FS
    k = np.log(f1 / f0) / SECS
    return (10 ** (db / 20) * np.sin(2 * np.pi * f0 * (np.exp(k * t) - 1) / k)).astype(np.float32)


def sig_drums(db=-6, seed=3):
    """Synthetic drum loop: kick (decaying 55 Hz), snare (noise burst + 190 Hz) and hats, 120 bpm, peaks ~db."""
    rng = np.random.default_rng(seed)
    n = int(SECS * FS); x = np.zeros(n); beat = int(0.5 * FS)
    t = np.arange(int(0.4 * FS)) / FS
    kick = np.sin(2 * np.pi * (55 * t + 60 * (1 - np.exp(-t * 30)) / 30)) * np.exp(-t * 8)
    snare = (rng.standard_normal(len(t)) * 0.6 + np.sin(2 * np.pi * 190 * t)) * np.exp(-t * 18)
    hat = rng.standard_normal(int(0.05 * FS)) * np.exp(-np.arange(int(0.05 * FS)) / FS * 80) * 0.3
    for i in range(0, n - len(t), beat):
        k = i // beat
        x[i:i + len(t)] += kick if k % 2 == 0 else snare
        h = i + beat // 2
        if h + len(hat) < n: x[h:h + len(hat)] += hat
    x *= 10 ** (db / 20) / np.max(np.abs(x))
    return x.astype(np.float32)


def sig_music(db=-10, seed=5):
    """A dense synthetic mix: bass line, chords (saw-ish partials), pink noise bed, drums; stereo."""
    rng = np.random.default_rng(seed)
    n = int(SECS * FS); t = np.arange(n) / FS
    notes = np.array([55, 65.4, 73.4, 49])
    f = notes[(t / 0.75).astype(int) % 4]
    bass = np.sin(2 * np.pi * np.cumsum(f) / FS) * 0.5
    chord = sum(np.sin(2 * np.pi * f * k * t) / k for f in (220, 277.2, 329.6) for k in range(1, 8)) * 0.08
    bed = tgp.noise(SECS, -30, seed=seed, pink=True)
    d = sig_drums(-3, seed)
    l = bass + chord + bed + d * 0.8; r = bass + chord * 0.8 + np.roll(bed, 37) + d * 0.8
    m = max(np.max(np.abs(l)), np.max(np.abs(r)))
    return (np.stack([l, r]) * 10 ** (db / 20) / m).astype(np.float32)


SIGNALS = {
    'pink -18': lambda: sig_noise(-18), 'pink -6': lambda: sig_noise(-6), 'sweep -12': lambda: sig_sweep(-12),
    'drums -6': lambda: sig_drums(-6), 'music -10': lambda: sig_music(-10), 'music -2': lambda: sig_music(-2),
}
N = tgp.NONE
CASES = [
    # name, signal, normalised params, displayed params
    ('path: all stages bypassed', 'pink -18', N, {}),
    ('eq: low +6 mid -4 high +3 (eq only)', 'pink -18', tgp.EQ_ONLY, {'EQ Low Gain': 6, 'EQ Mid Gain': -4, 'EQ High Gain': 3}),
    ('eq: sweep, low -6 high +6 (eq only)', 'sweep -12', tgp.EQ_ONLY, {'EQ Low Gain': -6, 'EQ High Gain': 6}),
    ('lim: pink -6, gain 5 (limiter only)', 'pink -6', tgp.LIM_ONLY, {}),
    ('lim: drums -6, gain 12 (limiter only)', 'drums -6', tgp.LIM_ONLY, {'Limiter Input Gain': 12}),
    ('lim: music -2, gain 5 (limiter only)', 'music -2', tgp.LIM_ONLY, {}),
    ('char: pink -18, 100 % (character only)', 'pink -18', tgp.CHAR_ONLY, {}),
    ('char: pink -6, 100 % (character only)', 'pink -6', tgp.CHAR_ONLY, {}),
    ('char: music -10, 50 %', 'music -10', {**tgp.CHAR_ONLY, 'Character': 0.25}, {}),
    ('char: music -10, 100 %', 'music -10', tgp.CHAR_ONLY, {}),
    ('char: music -10, 200 %', 'music -10', {**tgp.CHAR_ONLY, 'Character': 1.0}, {}),
    ('char: drums -6, 100 %', 'drums -6', tgp.CHAR_ONLY, {}),
    ('full: music -10, defaults', 'music -10', {}, {}),
    ('full: music -2, defaults', 'music -2', {}, {}),
    ('full: drums -6, defaults', 'drums -6', {}, {}),
    ('full: music -10, 200 %, gain 10, eq +3/-2/+4, in -3 out +2', 'music -10', {'Character': 1.0},
     {'Limiter Input Gain': 10, 'EQ Low Gain': 3, 'EQ Mid Gain': -2, 'EQ High Gain': 4, 'Input Gain': -3, 'Output Gain': 2}),
]


def residual_db(ref, ours):
    a, b = ref[:, PRE:], ours[:, PRE:]
    return 10 * np.log10(np.sum((a - b) ** 2) / max(np.sum(a ** 2), 1e-30))


def run(only=None, vst3=False):
    cases = [c for c in CASES if only is None or only in c[0]]
    sigs = {k: f() for k, f in SIGNALS.items()}
    pad = lambda x: np.concatenate([np.zeros((2, PRE) if x.ndim == 2 else PRE, np.float32), x], axis=-1)
    jobs = [dict(x=pad(sigs[s]), norm=nm, params=pm) for _, s, nm, pm in cases]
    ref = tgp.render_many(jobs, 'ref', cache=True)
    ours = tgp.render_many(jobs, 'ours' if vst3 else 'model')
    res = {}
    for (name, s, nm, pm), r, o in zip(cases, ref, ours):
        res[name] = dict(residual_dB=round(float(residual_db(r, o)), 2), ref_rms_dBFS=round(tgp.rms_db(r[:, PRE:]), 2),
                         ours_rms_dBFS=round(tgp.rms_db(o[:, PRE:]), 2))
        print(f'{res[name]["residual_dB"]:8.2f} dB   {name}   (rms ref {res[name]["ref_rms_dBFS"]}, ours {res[name]["ours_rms_dBFS"]})')
    return res


def report():
    """NULLTESTS.md from results.json (our processor) and results_vst3.json (the built VST3), when present"""
    here = os.path.dirname(os.path.abspath(__file__))
    model = json.load(open(os.path.join(here, 'results.json')))
    vst3p = os.path.join(here, 'results_vst3.json')
    vst3 = json.load(open(vst3p)) if os.path.exists(vst3p) else {}
    sigdoc = {'pink -18': 'pink noise, -18 dBFS RMS', 'pink -6': 'pink noise, -6 dBFS RMS', 'sweep -12': 'log sine sweep 20 Hz - 20 kHz, -12 dBFS',
              'drums -6': 'synthetic drum loop (kick, snare, hats), peaks -6 dBFS', 'music -10': 'synthetic mix (bass, chords, noise bed, drums), stereo, peaks -10 dBFS',
              'music -2': 'the same mix, peaks -2 dBFS'}
    lines = ['# Null tests', '',
             'The same signal and settings through the reference (Cradle The God Particle 1.2.4, VST3) and through The Antiparticle. The',
             "residual is reference minus ours, in dB re the reference's output energy. Lower is better: -20 dB means the difference",
             "is a tenth of the signal's amplitude, -60 dB is inaudible in practice. Both renders are compensated by each plug-in's",
             'reported latency (they report the same values), at 48 kHz, oversampling 1x, block 512. The first 2 s are skipped:',
             "the reference's delay settles after loading. Signals are synthetic and generated by `nulltest/nulltest.py` (no copyrighted",
             "audio); stages are isolated with the reference's own power buttons.", '',
             'Reproduce: `python nulltest/nulltest.py` (our processor through tools/ApRender) and `python nulltest/nulltest.py --vst3`',
             '(the built VST3 through tools/RefRender). The reference renders stay local (git-ignored).', '',
             '| Case | Signal | Residual, ours (dB) | Residual, VST3 build (dB) |', '|---|---|---|---|']
    for name, sig, nm, pm in CASES:
        r = model.get(name, {}); v = vst3.get(name, {})
        lines.append(f'| {name.split(" (")[0]} | {sigdoc[sig]} | {r.get("residual_dB", "")} | {v.get("residual_dB", "")} |')
    lines += ['', 'Signal list: ' + '; '.join(f'**{k}**: {v}' for k, v in sigdoc.items()) + '.', '',
              '## What the numbers say', '',
              'None of these is a perfect null; the full chain lands between about -36 and -41 dB on program material.',
              '',
              '- **The path, EQ and crossover are exact for practical purposes** (-79 to -87 dB): the fixed delay, the RBJ',
              '  filters and the character section\'s FIR crossover (taken from the reference\'s own impulse responses) are',
              '  linear and fully identified. At amount 0 % the character section nulls at -137 dB.',
              '- **The character section is the main residual** (-34 to -46 dB). Its structure (parallel compressor and',
              '  cubic shaper per band) and constants are fitted, not known. It is best at moderate amounts and levels and',
              '  worst at 200 % and on loud dense material. Most of what is left is in the low band (bass under 160 Hz).',
              '- **The limiter is close on program material** (-45 dB on the mix at -2 dBFS) but not under heavy, continuous',
              '  limiting (-26 to -28 dB on loud pink noise and drums with +12 dB of input gain). Its slow release is',
              '  identified exactly (MEASUREMENTS.md 4); how its fast stage ramps between closely spaced peaks is not.',
              '- **The built VST3 and the offline renderer give the same numbers**, so these hold for the plug-in itself.',
              '']
    osp = os.path.join(here, 'results_oversampling.json')
    if os.path.exists(osp):
        lines += ['## Oversampling', '',
                  "Default settings with the reference switched to 2x / 4x / 8x through its own settings page (then back to 1x) and",
                  'ours forced to the same ratio (`python nulltest/null_oversampling.py`).', '',
                  '| Ratio, signal | Residual, ours (dB) |', '|---|---|']
        for name, v in json.load(open(osp)).items():
            lines.append(f'| {name} | {v["residual_dB"]} |')
        lines.append('')
    return lines


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--only'); ap.add_argument('--vst3', action='store_true'); ap.add_argument('--report', action='store_true')
    a = ap.parse_args()
    if a.report:
        open(os.path.join(tgp.ROOT, 'NULLTESTS.md'), 'w').write('\n'.join(report()))
        raise SystemExit
    res = run(a.only, a.vst3)
    if a.only is None:
        json.dump(res, open(os.path.join(os.path.dirname(__file__), 'results.json' if not a.vst3 else 'results_vst3.json'), 'w'), indent=1)
