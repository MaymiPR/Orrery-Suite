"""The limiter alone at another rate (default 44.1 kHz): loud drums, pink noise and the demo track, with one tuning
constant changed at a time (window, release, reach, slow envelope), to find what is set differently at that rate.

    python limrate.py [fs] [key=v1,v2,... ...]"""
import sys
import numpy as np
import tgp, nulltest as N, track as T

if __name__ == '__main__':
    fs = int(sys.argv[1]) if len(sys.argv) > 1 and '=' not in sys.argv[1] else 44100
    scans = [a for a in sys.argv[1:] if '=' in a] or ['limWindowAt=18,19,20,21', 'limReleaseAt=18,19,20,21,22',
                                                     'limReach=19,21,22.9,25', 'limSlowAttackMs=22.5,24.47,26.6',
                                                     'limSlowReleaseMs=347,377.8,411']
    xt, fst = T.load(T.WAV)
    pre = 2 * fs
    pad = lambda x: np.concatenate([np.zeros((2, pre), np.float32), (np.vstack([x, x]) if x.ndim == 1 else x).astype(np.float32)], axis=1)
    sigs = [pad(N.sig_drums(-1)), pad(N.sig_noise(-6)), pad(xt[:, 30 * fst:36 * fst])]
    ref = tgp.render_many([dict(x=x, norm=tgp.LIM_ONLY) for x in sigs], 'ref', fs=fs, cache=True)
    def score(tune):
        ours = tgp.render_many([dict(x=x, norm=tgp.LIM_ONLY, tune=tune) for x in sigs], 'model', fs=fs)
        return [10 * np.log10(np.sum((r[:, pre:] - o[:, pre:]) ** 2) / np.sum(r[:, pre:] ** 2)) for r, o in zip(ref, ours)]
    print('now        ' + ' '.join(f'{v:7.2f}' for v in score({})) + '   (drums -1, pink -6, track)', flush=True)
    for sc in scans:
        k, vals = sc.split('=')
        for v in vals.split(','):
            print(f'{k}={v:8s} ' + ' '.join(f'{s:7.2f}' for s in score({k: float(v)})), flush=True)
