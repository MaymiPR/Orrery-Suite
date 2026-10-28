"""Probes the oversampled path's character section at other host rates: the crossover's source rate
(Tuning osHostFilters; HF=0,1 picks which) and the path's delay (osDelayAdd), drums -6 and pink -18 against the cached reference renders
of null_oversampling.py.  python os_probe.py fs k"""
import os, sys
import numpy as np
import tgp, nulltest as NT

PRE = NT.PRE
pad = lambda x: np.concatenate([np.zeros((2, PRE) if x.ndim == 2 else PRE, np.float32), x], axis=-1)
SIGS = {'drums -6': pad(NT.sig_drums(-6)), 'pink -18': pad(NT.sig_noise(-18))}

if __name__ == '__main__':
    fs, k = int(sys.argv[1]), int(sys.argv[2])
    ref = tgp.render_many([dict(x=x, salt=f'os{k}') for x in SIGS.values()], 'ref', fs=fs, cache=True)
    for hf in [int(h) for h in os.environ.get('HF', '0,1').split(',')]:
        for add in [float(a) for a in (sys.argv[3:] or ['-0.5', '-0.25', '0', '0.25', '0.5'])]:
            ours = tgp.render_many([dict(x=x, tune={'oversampling': k, 'osHostFilters': hf, 'osDelayAdd': add}) for x in SIGS.values()], 'model', fs=fs)
            print(f'{fs} {k}x host filters {hf} delay {add:+.3f}:', ' '.join(f'{n} {NT.residual_db(r, o):.2f}' for n, r, o in zip(SIGS, ref, ours)), flush=True)
