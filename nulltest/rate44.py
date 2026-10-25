"""Why 44.1 kHz nulls worse than 48 kHz: loud drums, pink noise and the demo track at both rates, each stage alone and
the defaults, with the residual split into the three bands (FFT masks at the crossovers).

    python rate44.py [rates ...] [--tune key=value ...]     (tune: Tuning overrides for our processor, e.g. high.attack.8=0.8)"""
import sys
import numpy as np
import tgp, nulltest as N, track as T, trackdiag as TD

if __name__ == '__main__':
    tune = dict((a.split('=')[0], float(a.split('=')[1])) for a in sys.argv[sys.argv.index('--tune') + 1:]) if '--tune' in sys.argv else {}
    xt, fst = T.load(T.WAV)
    sigs = {'drums -1': N.sig_drums(-1), 'pink -6': N.sig_noise(-6), 'track 30 s': xt[:, 30 * fst:36 * fst]}
    stages = {'char only': tgp.CHAR_ONLY, 'lim only': tgp.LIM_ONLY, 'defaults': {}}
    for fs in [int(a) for a in sys.argv[1:] if a.isdigit()] or (44100, 48000):
        pre = 2 * fs
        jobs, names = [], []
        for sn, x in sigs.items():
            x = np.atleast_2d(x); x = np.vstack([x, x]) if x.shape[0] == 1 else x
            xp = np.concatenate([np.zeros((2, pre), np.float32), x.astype(np.float32)], axis=1)
            for st, nm in stages.items():
                jobs.append(dict(x=xp, norm=nm, tune=tune)); names.append((sn, st))
        ref = tgp.render_many([{k: v for k, v in j.items() if k != 'tune'} for j in jobs], 'ref', fs=fs, cache=True)
        ours = tgp.render_many(jobs, 'model', fs=fs)
        for (sn, st), r, o in zip(names, ref, ours):
            a, b = r[:, pre:].astype(np.float64), o[:, pre:].astype(np.float64)
            e = a - b
            eb, ab = TD.split(e, fs), TD.split(a, fs)
            print(f'{fs} {sn:10s} {st:9s} {10 * np.log10(np.sum(e ** 2) / np.sum(a ** 2)):7.2f} dB | per band ' +
                  ' '.join(f'{10 * np.log10(p / q):7.2f}' for p, q in zip(eb, ab)) + ' | share ' +
                  ' '.join(f'{100 * p / sum(eb):3.0f}%' for p in eb), flush=True)
