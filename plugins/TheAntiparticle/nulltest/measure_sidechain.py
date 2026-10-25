"""Side-chain EXT (a host-only setting, set through the saved state): default chain, a steady 1 kHz main tone, and
side-chain bursts. Gain on the main tone with the side-chain quiet vs loud, INT vs EXT; and whether the main
signal still drives the dynamics in EXT. Writes measurements/sidechain/sidechain.json."""
import json, os
import numpy as np
import tgp, tgpstate

OUT = os.path.join(tgp.ROOT, 'measurements', 'sidechain'); os.makedirs(OUT, exist_ok=True)
FS = tgp.FS
PRE = 2 * FS
REF = os.path.join(tgp.ROOT, 'reference')


def lockin(y, f, a, b):
    w = np.hanning(b - a); t = np.arange(a, b) / FS
    return abs(np.sum(y[a:b] * w * np.exp(-2j * np.pi * f * t)) / np.sum(w) * 2)


if __name__ == '__main__':
    s = open(os.path.join(REF, 'state_default.bin'), 'rb').read()
    ext = os.path.join(REF, 'state_sidechain_ext.bin')
    open(ext, 'wb').write(tgpstate.edit(s, b'sidechain = 0', b'sidechain = 1'))
    intl = os.path.join(REF, 'state_default.bin')
    n = PRE + 6 * FS
    sil = np.zeros(n)
    burst = lambda f: np.concatenate([np.zeros(PRE + 2 * FS), tgp.tone(f, 2, -3), np.zeros(2 * FS)])
    main = lambda db: np.concatenate([np.zeros(PRE), tgp.tone(1000, 6, db)])
    cases = {
        'EXT, main -40, sc 1.1 kHz -3': (ext, main(-40), burst(1100)),
        'INT, main -40, sc 1.1 kHz -3': (intl, main(-40), burst(1100)),
        'EXT, main -40, sc 8 kHz -3': (ext, main(-40), burst(8000)),
        'EXT, main -40, sc 60 Hz -3': (ext, main(-40), burst(60)),
        'EXT, main -20, sc silent': (ext, main(-20), sil),
        'INT, main -20, sc silent': (intl, main(-20), sil),
        'EXT, main -40, sc silent': (ext, main(-40), sil),
    }
    jobs = [dict(x=x, sc=sc, state=st, raw=True) for st, x, sc in cases.values()]
    ys = tgp.render_many(jobs, cache=True)
    res = {}
    for (k, (st, x, sc)), y in zip(cases.items(), ys):
        lvl = -40 if 'main -40' in k else -20
        g = lambda a, b: round(float(20 * np.log10(lockin(y[0], 1000, PRE + int(a * FS), PRE + int(b * FS)))) - lvl, 3)
        res[k] = dict(gain_before_dB=g(1.0, 1.9), gain_during_sc_burst_dB=g(3.0, 3.9), gain_after_dB=g(5.0, 5.9))
        print(k, res[k])
    json.dump(res, open(os.path.join(OUT, 'sidechain.json'), 'w'), indent=1)
