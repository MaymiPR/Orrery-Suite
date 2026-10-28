"""Null tests with oversampling: the reference is switched to 2x / 4x / 8x through its own settings page
(measure_oversampling.set_factor, restored to 1x at the end), ours by forcing the same ratio. Defaults, the synthetic
mix and the drum loop, at 48 kHz and (the filters follow the host rate) 44.1 and 96 kHz. Writes nulltest/results_oversampling.json."""
import json, os
import numpy as np
import tgp, nulltest as NT
from measure_oversampling import set_factor

PRE = NT.PRE

if __name__ == '__main__':
    pad = lambda x: np.concatenate([np.zeros((2, PRE) if x.ndim == 2 else PRE, np.float32), x], axis=-1)
    sigs = {'music -10': pad(NT.sig_music(-10)), 'drums -6': pad(NT.sig_drums(-6)), 'pink -18': pad(NT.sig_noise(-18))}
    res = {}
    cached = lambda k, fs: all(os.path.exists(os.path.join(tgp.CACHE, tgp._key(dict(x=x, salt=f'os{k}'), 'ref', fs, 512) + '.npy')) for x in sigs.values())
    switched = False
    try:
        for k in (2, 4, 8):
            for fs in (48000, 44100, 96000):
                if not cached(k, fs):        # the reference only has to be switched when its renders are not cached yet
                    set_factor(k); switched = True
                lat = tgp.latency(fs=fs)
                jobs = [dict(x=x, salt=f'os{k}') for x in sigs.values()]
                ref = tgp.render_many(jobs, 'ref', fs=fs, cache=True)
                ours = tgp.render_many([dict(x=x, tune={'oversampling': k}) for x in sigs.values()], 'model', fs=fs)
                at = '' if fs == 48000 else f' at {fs / 1000:g} kHz'
                for (name, _), r, o in zip(sigs.items(), ref, ours):
                    key = f'{k}x, defaults, {name}{at}'
                    res[key] = dict(residual_dB=round(float(NT.residual_db(r, o)), 2), reference_latency=lat)
                    print(f'{key}: {res[key]["residual_dB"]} dB (reference latency {lat})', flush=True)
    finally:
        if switched:
            set_factor(1)
            print('restored 1x, latency', tgp.latency())
    json.dump(res, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'results_oversampling.json'), 'w'), indent=1)
