"""Stress null tests: the reference against our processor over a wide grid of settings, levels, signals, sample rates,
block sizes and parameter automation, beyond the fixed cases of nulltest.py. Amounts and gains are taken between the
fitted nodes on purpose (12.5 % steps, odd dB values), so interpolation is tested too.

    python stress.py                    # every group
    python stress.py --only amount      # groups whose name contains 'amount'
    python stress.py --vst3             # our built VST3 instead of ApRender
Writes nulltest/results_stress.json; nulltest.py --report adds it to NULLTESTS.md."""
import argparse, json, os
import numpy as np
import tgp
import nulltest as N

FS = tgp.FS
PRE = N.PRE


def sig_bass(db=-6):
    """bass-heavy: a sub line with slides and a kick"""
    t = np.arange(int(N.SECS * FS)) / FS
    f = 40 + 25 * (np.sin(2 * np.pi * 0.4 * t) > 0) + 10 * np.sin(2 * np.pi * 0.13 * t)
    x = np.sin(2 * np.pi * np.cumsum(f) / FS) * 0.7 + N.sig_drums(-6, 9) * 0.6
    return (x * 10 ** (db / 20) / np.max(np.abs(x))).astype(np.float32)


def sig_bright(db=-8, seed=21):
    """treble-heavy: dense hats, sibilant noise bursts and a 6 - 12 kHz tone cluster"""
    rng = np.random.default_rng(seed)
    n = int(N.SECS * FS); t = np.arange(n) / FS
    x = sum(np.sin(2 * np.pi * f * t + rng.uniform(0, 6.28)) for f in (6100, 7900, 9300, 11800)) * 0.15
    hp = np.diff(rng.standard_normal(n + 1))
    env = (np.sin(2 * np.pi * 2.5 * t) > 0.6) * 1.0
    x = x + hp * env * 0.4 + np.diff(N.sig_drums(-6, 4), prepend=0) * 2
    return (x * 10 ** (db / 20) / np.max(np.abs(x))).astype(np.float32)


def sig_wide(db=-10):
    """the music mix with extreme stereo: right channel partly inverted, plus a left-only lead"""
    m = N.sig_music(db, seed=8)
    t = np.arange(m.shape[1]) / FS
    lead = 0.25 * np.sin(2 * np.pi * 880 * t) * (np.sin(2 * np.pi * 0.5 * t) > 0)
    l, r = m[0] + lead * 10 ** (db / 20), -0.7 * m[1] + 0.3 * m[0]
    return np.stack([l, r]).astype(np.float32)


def sig_dynamic(db=-4):
    """quiet and loud passages: the mix at -30, -10 and the given level, 2 s each (detector settling)"""
    m = N.sig_music(0, seed=12)
    n = m.shape[1] // 3
    g = np.concatenate([np.full(n, -30), np.full(n, -10), np.full(m.shape[1] - 2 * n, db)])
    return (m * 10 ** (g / 20)).astype(np.float32)


SIG = {
    'music -10': lambda: N.sig_music(-10), 'music -2': lambda: N.sig_music(-2), 'music -20': lambda: N.sig_music(-20),
    'music 0': lambda: N.sig_music(0), 'drums -6': lambda: N.sig_drums(-6), 'drums -1': lambda: N.sig_drums(-1),
    'pink -18': lambda: N.sig_noise(-18), 'pink -10': lambda: N.sig_noise(-10), 'pink -6': lambda: N.sig_noise(-6),
    'bass -6': lambda: sig_bass(-6), 'bright -8': lambda: sig_bright(-8), 'wide -10': lambda: sig_wide(-10),
    'dynamic -4': lambda: sig_dynamic(-4), 'sweep -6': lambda: N.sig_sweep(-6),
}


def amt(p):
    return p / 200.0          # Character, normalised (0 .. 200 %)


def groups():
    """(group, [(case name, signal, norm, params, auto)])"""
    g = []
    g.append(('amount', [(f'amount {a} %, music -10', 'music -10', {'Character': amt(a)}, {}, None)
                         for a in (0, 12.5, 37.5, 62.5, 87.5, 100, 112.5, 137.5, 162.5, 187.5, 200)]))
    g.append(('amount char-only', [(f'char only {a} %, pink -10', 'pink -10', {**tgp.CHAR_ONLY, 'Character': amt(a)}, {}, None)
                                   for a in (25, 62.5, 100, 150, 200)]))
    g.append(('limiter gain', [(f'limiter {v} dB, {s}', s, {}, {'Limiter Input Gain': v}, None)
                               for s in ('music -2', 'drums -6') for v in (0, 2.5, 5, 7.5, 10, 12)]))
    eqs = [(6, 0, 0), (-6, 0, 0), (0, 6, 0), (0, -6, 0), (0, 0, 6), (0, 0, -6), (6, -6, 6), (-6, 6, -6), (3.5, -1.5, 4.5)]
    g.append(('eq', [(f'eq {l:+g}/{m:+g}/{h:+g}, music -10', 'music -10', {},
                      {'EQ Low Gain': l, 'EQ Mid Gain': m, 'EQ High Gain': h}, None) for l, m, h in eqs]))
    g.append(('io gain', [(f'input {i:+g} output {o:+g}, music -10', 'music -10', {}, {'Input Gain': i, 'Output Gain': o}, None)
                          for i, o in ((-12, 0), (-6, 3), (6, -6), (12, -12), (0, 12), (9.5, 0))]))
    g.append(('levels', [(f'defaults, {s}', s, {}, {}, None)
                         for s in ('music -20', 'music 0', 'drums -1', 'pink -18', 'pink -6', 'bass -6', 'bright -8',
                                   'wide -10', 'dynamic -4', 'sweep -6')]))
    g.append(('combos', [
        ('200 %, lim 12, eq +6/+6/+6, in +6, music -10', 'music -10', {'Character': 1.0}, {'Limiter Input Gain': 12, 'EQ Low Gain': 6, 'EQ Mid Gain': 6, 'EQ High Gain': 6, 'Input Gain': 6}, None),
        ('150 %, lim 0, eq -6/0/+6, bass -6', 'bass -6', {'Character': 0.75}, {'Limiter Input Gain': 0, 'EQ Low Gain': -6, 'EQ High Gain': 6}, None),
        ('25 %, lim 8, bright -8', 'bright -8', {'Character': 0.125}, {'Limiter Input Gain': 8}, None),
        ('175 %, wide -10, eq +4/-3/+2', 'wide -10', {'Character': 0.875}, {'EQ Low Gain': 4, 'EQ Mid Gain': -3, 'EQ High Gain': 2}, None),
        ('char + lim (eq off), 100 %, drums -1', 'drums -1', {'EQ Bypass': 1}, {}, None),
        ('eq + lim (char off), eq +6/-6/+6, music -2', 'music -2', {'Character Bypass': 1}, {'EQ Low Gain': 6, 'EQ Mid Gain': -6, 'EQ High Gain': 6}, None),
        ('eq + char (lim off), 200 %, pink -10', 'pink -10', {'Limiter Bypass': 1, 'Character': 1.0}, {'EQ High Gain': 6}, None),
    ]))
    s = lambda sec: PRE + int(sec * FS)
    g.append(('automation', [
        ('amount sweep 0 -> 200 %, music -10', 'music -10', {'Character': 0.0}, {},
         [('Character', s(0.5 + i * 0.05), min(1.0, i / 80)) for i in range(81)]),
        ('amount jumps 50/150/0/200 %, music -10', 'music -10', {'Character': 0.25}, {},
         [('Character', s(1), 0.75), ('Character', s(2.2), 0.0), ('Character', s(3.5), 1.0)]),
        ('input gain jumps -12/+12, music -10', 'music -10', {}, {},
         [('Input Gain', s(1), 0.0), ('Input Gain', s(2.5), 1.0), ('Input Gain', s(4), 0.5)]),
        ('limiter gain ramp 0 -> 12 dB, music -2', 'music -2', {}, {'Limiter Input Gain': 0},
         [('Limiter Input Gain', s(0.5 + i * 0.05), i / 80) for i in range(81)]),
        ('eq moves, music -10', 'music -10', {}, {},
         [('EQ Low Gain', s(1), 1.0), ('EQ High Gain', s(2), 0.0), ('EQ Mid Gain', s(3), 0.8), ('EQ Low Gain', s(4), 0.3)]),
        ('output gain jumps, drums -6', 'drums -6', {}, {}, [('Output Gain', s(1), 0.1), ('Output Gain', s(3), 0.9)]),
        ('bypass toggles: char, lim, eq, music -10', 'music -10', {}, {'EQ High Gain': 4},
         [('Character Bypass', s(1), 1), ('Character Bypass', s(2), 0), ('Limiter Bypass', s(2.5), 1),
          ('Limiter Bypass', s(3.5), 0), ('EQ Bypass', s(4), 1), ('EQ Bypass', s(5), 0)]),
    ]))
    return g


RUNS = [('48 kHz, block 512', 48000, 512, None), ('44.1 kHz', 44100, 512, ['amount', 'levels']),
        ('96 kHz', 96000, 512, ['amount', 'levels']), ('88.2 kHz', 88200, 512, ['levels']),
        ('block 64', 48000, 64, ['levels', 'automation']), ('block 4096', 48000, 4096, ['levels', 'automation']),
        ('block 333', 48000, 333, ['levels'])]


def run(only=None, vst3=False):
    sigs = {k: f() for k, f in SIG.items()}
    pad = lambda x: np.concatenate([np.zeros((2, PRE) if x.ndim == 2 else PRE, np.float32), x], axis=-1)
    res = {}
    for run_name, fs, block, which in RUNS:
        cases = [(gname, c) for gname, cs in groups() for c in cs
                 if (which is None or gname in which) and (only is None or only in gname or only in run_name)]
        if not cases: continue
        jobs = []
        for gname, (name, s, norm, params, auto) in cases:
            j = dict(x=pad(sigs[s]), norm=norm, params=params)
            if auto: j['auto'] = [(k, int(round(t * fs / FS)), v) for k, t, v in auto]
            jobs.append(j)
        ref = tgp.render_many(jobs, 'ref', fs=fs, block=block, cache=True)
        ours = tgp.render_many(jobs, 'ours' if vst3 else 'model', fs=fs, block=block)
        print(f'== {run_name}', flush=True)
        for (gname, (name, s, *_)), r, o in zip(cases, ref, ours):
            d = round(float(N.residual_db(r, o)), 2)
            res[f'{run_name} | {gname} | {name}'] = d
            print(f'{d:8.2f} dB   [{gname}] {name}', flush=True)
    return res


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--only'); ap.add_argument('--vst3', action='store_true')
    a = ap.parse_args()
    res = run(a.only, a.vst3)
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'results_stress.json')
    old = json.load(open(out)) if os.path.exists(out) and a.only else {}
    json.dump({**old, **res}, open(out, 'w'), indent=1)
    vals = list(res.values())
    print(f'{len(vals)} cases: median {np.median(vals):.1f} dB, worst {max(vals):.1f} dB')
