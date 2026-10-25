"""Which gain-law structure can follow the high band's steady gain curve: staticn.py's staircases (tone, two tones,
band noise), the reference's band gain per step read through the measured shaper, fitted with the plug-in's gain
computer (charsim) in several forms: the soft knee as now, a dB-domain mix (n = g^mix), a feedback detector, a true
peak detector, and a hard knee. Prints the rms error per form (dB, levels -18 ... +3 dBFS).

    python staticfit.py [amount 0..1]"""
import os, sys
import numpy as np
from scipy.optimize import least_squares
import tgp, charmodel as CM, charsim as C, ident as I, staticn as S
from hightrace2 import table_laws

LO, HI = -18, 3

if __name__ == '__main__':
    amt = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5
    row = int(round(amt * 16))
    ref = np.load(os.path.join(S.D, f'staticn_{amt}_8000.npy'))[:, 0]           # (kind, level) dB
    sel = [i for i, l in enumerate(S.LEVELS) if LO <= l <= HI]
    sigs = []
    for kind in ('tone', 'twotone', 'noise'):
        x, n = S.staircase(kind, 8000.0)
        sb = I.nl_bands(x)[2, 0]
        sigs.append((sb, n))
    law = table_laws(row)['high']
    base = {k: v for k, v in law.items() if k in C.NAMES and k not in ('makeup', 'width', 'cubic', 'shapeDry', 'shapeMid', 'side')}

    def curves(p, peak=1, feedback=False):
        out = []
        for sb, n in sigs:
            g = C.band_gain(sb, p, peak=peak, feedback=feedback)
            out.append([20 * np.log10(np.median(g[I.PRE + i * n + n * 2 // 3:I.PRE + (i + 1) * n])) for i in range(len(S.LEVELS))])
        return np.array(out)

    keys = ['mix', 'thresh', 'knee', 'ratio', 'attack', 'release']
    lo = [0, -30, 0, 1, 0.01, 1]; hi = [1, 0, 30, 30, 50, 3000]
    forms = {
        'soft knee (now)': dict(),
        'mix in dB': dict(mixDb=1.0),
        'feedback': dict(feedback=True),
        'true peak': dict(peak=2),
        'hard knee': dict(hard=True),
        'mix in dB, feedback': dict(mixDb=1.0, feedback=True),
    }
    now = curves(base)
    print('current law: rms %.3f dB' % np.sqrt(np.mean((now[:, sel] - ref[:, sel]) ** 2)))
    for name, f in forms.items():
        def model(q):
            p = dict(base, **dict(zip(keys, q)))
            if f.get('hard'): p['knee'] = 0.0
            if 'mixDb' in f: p['mixDb'] = 1.0
            return curves(p, f.get('peak', 1), f.get('feedback', False))
        q0 = [base[k] for k in keys]
        if 'mixDb' in f: lo[0], hi[0], q0[0] = 0, 3, 1.0
        else: lo[0], hi[0] = 0, 1
        r = least_squares(lambda q: (model(q)[:, sel] - ref[:, sel]).ravel(), q0, bounds=(lo, hi), x_scale='jac', max_nfev=200)
        err = (model(r.x)[:, sel] - ref[:, sel])
        print(f'{name:22s} rms {np.sqrt(np.mean(err ** 2)):.3f} dB  (tone {np.sqrt(np.mean(err[0] ** 2)):.3f}, two tones {np.sqrt(np.mean(err[1] ** 2)):.3f}, noise {np.sqrt(np.mean(err[2] ** 2)):.3f})  '
              + ' '.join(f'{k} {v:.3g}' for k, v in zip(keys, r.x)), flush=True)
