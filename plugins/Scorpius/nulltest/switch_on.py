"""What a module of the reference does when switched back on (autom_probe.py's '<module> power' cases: off at 1 s,
on at 2 s). Its output after the switch is compared with three stand-ins rendered with the module on throughout:
    steady   the same input, never switched (what it settles to)
    silence  the input silent before the switch (a model starting from silence)
    paused   the input with the second it was off cut out (a model carrying on from where it stopped)
Prints the difference in dB below the output in windows after the switch, for the reference and for ours.
    python switch_on.py [module drive|harm|comp|lift] [block] [drive colour a|b|c]"""
import json, os, sys
import numpy as np
import orion as o
from autom_probe import PRE, T1, T2, cases
from order import drums

EDGES = [0, 96, 240, 480, 960, 1920, 4800, 9600, 19200]          # window edges, samples after the switch


def switch_at(t, block, chunk=128):
    """the sample a switch requested at t takes effect: the start of its host block, plus the wait for a whole
    control chunk with blocks under one (MEASUREMENTS.md 12)"""
    return t // block * block + (-(-(chunk - block) // block) * block if block < chunk else 0)


def table(y, refs, t):
    rows = {}
    for k, z in refs.items():
        rows[k] = []
        for a, b in zip(EDGES[:-1], EDGES[1:]):
            s, e = t + a, t + b
            rows[k].append(10 * np.log10(np.sum((y[:, s:e] - z[:, s:e]) ** 2) / (np.sum(y[:, s:e] ** 2) + 1e-30) + 1e-30))
    return rows


if __name__ == '__main__':
    module = sys.argv[1] if len(sys.argv) > 1 else 'drive'
    block = int(sys.argv[2]) if len(sys.argv) > 2 else 512
    colour = sys.argv[3] if len(sys.argv) > 3 else 'a'
    x = np.stack([drums()[:PRE + 3 * o.FS]] * 2) * 10 ** (-12 / 20)
    on, p, _, _ = cases()[module + ' power']
    hidden = {'drive.driveMode': colour} if module == 'drive' else {}
    tune = json.loads(os.environ.get('TUNE', '{}'))
    s1, s2 = switch_at(PRE + T1, block), switch_at(PRE + T2, block)
    quiet = x.copy(); quiet[:, :s2] = 0
    # the paused stand-in: the drive hears x up to the switch off, then x from the switch on
    cut = np.concatenate([x[:, :s1], x[:, s2:]], axis=1)
    jobs = [dict(x=x, params=on, hidden=hidden, auto=[(p, PRE + T1, 0), (p, PRE + T2, 1)]),
            dict(x=x, params=on, hidden=hidden), dict(x=quiet, params=on, hidden=hidden), dict(x=cut, params=on, hidden=hidden)]
    print('windows after the switch (samples):', ' '.join(f'{a}-{b}' for a, b in zip(EDGES[:-1], EDGES[1:])))
    for who in ('ref', 'model'):
        js = jobs if who == 'ref' else [dict(j, tune=tune) for j in jobs]
        y, st, si, pa = o.render_many(js, who, block=block, cache=who == 'ref')
        pa = np.concatenate([np.zeros((2, s2 - s1), np.float32), pa], axis=1)[:, :y.shape[1]]
        for k, row in table(y, dict(steady=st, silence=si, paused=pa), s2).items():
            print(f'{who:5s} vs {k:8s}', ' '.join(f'{v:7.1f}' for v in row))
        if who == 'ref': ref = y
    print('ours vs reference     ', ' '.join(f'{v:7.1f}' for v in table(y, dict(r=ref), s2)['r']))
