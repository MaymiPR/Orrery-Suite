"""Automation against the reference: a parameter jumps at 1 s and back at 2 s (host blocks of 512 and 64 samples);
the null over the whole render and over 0.15 s windows around each jump, to see whether our smoothing and its timing
match (The God Particle's framework applies parameters per 128-sample chunk, MEASUREMENTS.md there).
    TUNE='{"switchMs": 0}' python autom_probe.py [case ...]"""
import json, os, sys
import numpy as np
import orion as o
from order import drums

PRE = 4800
T1, T2 = 48000, 96000                 # jump times (samples after the pre-roll)


def cases():
    lo = lambda *m: o.ONLY(*m)
    return {
        'comp amount': (dict(lo('comp'), **{'Comp Amount': 0.2}), 'Comp Amount', 0.2, 0.9),
        'harm amount': (dict(lo('harm'), **{'Harm Amount': 0.2}), 'Harm Amount', 0.2, 0.9),
        'lift amount': (dict(lo('lift'), **{'Lift Amount': 0.2}), 'Lift Amount', 0.2, 0.9),
        'comp power': (dict(lo('comp'), **{'Comp Amount': 0.8}), 'Comp On/Off', 1, 0),
        'lift power': (dict(lo('lift'), **{'Lift Amount': 0.8}), 'Lift On/Off', 1, 0),
        'loud power': (dict(lo('loud'), **{'Par Loud Level': 0.731}), 'Par Loud On/Off', 1, 0),
        'eq gain': (dict(lo('eq'), **{'EQ 100Hz': 1, 'EQ 3kHz': 3}), 'EQ 1kHz', 0.1, 0.8),
        'loud level': (dict(lo('loud'), **{'Par Loud Level': 0.2}), 'Par Loud Level', 0.2, 0.9),
        'harm power': (dict(lo('harm'), **{'Harm Amount': 0.8}), 'Harm On/Off', 1, 0),
        'drive power': (dict(lo('drive')), 'Drive On/Off', 1, 0),
        'eq power': (dict(lo('eq'), **{'EQ 100Hz': 4, 'EQ 3kHz': 3}), 'EQ On/Off', 1, 0),
    }


def win(r, y, t, a=-0.03, b=0.12):
    s, e = PRE + t + int(a * o.FS), PRE + t + int(b * o.FS)
    return 10 * np.log10(np.sum((r[:, s:e] - y[:, s:e]) ** 2) / np.sum(r[:, s:e] ** 2) + 1e-30)


if __name__ == '__main__':
    x = drums()[:, :PRE + 3 * o.FS] * 10 ** (-12 / 20) if drums().ndim == 2 else drums()[:PRE + 3 * o.FS] * 10 ** (-12 / 20)
    sel = sys.argv[1:] or list(cases())
    for block in (512, 64):
        for name in sel:
            params, p, v0, v1 = cases()[name]
            job = dict(x=x, params=params, auto=[(p, PRE + T1, v1), (p, PRE + T2, v0)])
            tune = json.loads(os.environ.get('TUNE', '{}'))          # tuning overrides for ours (probes)
            r = o.render_many([job], 'ref', block=block, cache=True)[0]
            y = o.render_many([dict(job, tune=tune)], 'model', block=block)[0]
            st = o.render_many([dict(x=x, params=params, tune=tune)], 'model', block=block)[0]
            rs = o.render_many([dict(x=x, params=params)], 'ref', block=block, cache=True)[0]
            print(f'block {block:3d} {name:12s}: whole {o.null_db(r, y):6.1f}  static {o.null_db(rs, st):6.1f}  '
                  f'jump1 {win(r, y, T1):6.1f}  jump2 {win(r, y, T2):6.1f}  steady {win(r, y, T1 + 24000):6.1f}', flush=True)
