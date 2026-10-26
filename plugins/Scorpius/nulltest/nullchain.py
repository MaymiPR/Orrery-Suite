"""Whole-plug-in nulls of our processor ('model' through ScRender, or 'ours' the VST3 build) against the reference:
the factory presets and a few settings, on the drum-like signal and the local track, at 48 and 44.1 kHz. The table
of NULLTESTS.md.
    python nullchain.py [model|ours]"""
import sys
import numpy as np
import orion as o
import nullmod as N

SETTINGS = {
    'start me up (default)': {},
    'c mode': dict(hidden={'drive.driveMode': 'c', 'ids.compression.mode': 'Mode A'}),
    'free at last': dict(params={'Comp On/Off': 0}),
    'louder': dict(params={'Harm Freq': 50, 'Lift Amount': 1.0, 'Par Loud On/Off': 1, 'Par Loud Level': 0.731}),
    'low end theory': dict(params={'Drive Hi-Pass': 0, 'Harm Freq': 50, 'EQ On/Off': 1, 'EQ 40Hz': 6}),
    'parallel me in': dict(params={'Harm Amount': 1.0, 'Comp Amount': 1.0, 'Lift Amount': 1.0}),
    'top of the morning': dict(params={'Drive On/Off': 0, 'Harm On/Off': 0, 'Comp On/Off': 0, 'Lift On/Off': 0,
                                       'Harm Amount': 1, 'Comp Amount': 1, 'Lift Amount': 1, 'Par Grit On/Off': 1, 'Par Grit Level': 0.731}),
    'all off': dict(params=o.NONE),
}

if __name__ == '__main__':
    plugin = sys.argv[1] if len(sys.argv) > 1 else 'model'
    sig = N.signals()
    for fs in (48000, 44100):
        print(f'--- {fs} Hz')
        for name, s in SETTINGS.items():
            jobs = []
            for x in sig.values():
                xx = x
                if fs != 48000:
                    import scipy.signal as sg
                    xx = sg.resample_poly(x, 147, 160, axis=-1).astype(np.float32)
                jobs.append(dict(x=xx, **s))
            ref = o.render_many(jobs, fs=fs, cache=True)
            ours = o.render_many(jobs, plugin=plugin, fs=fs)
            n = [o.null_db(r, m) for r, m in zip(ref, ours)]
            print(f'{name:24s}', '  '.join(f'{k} {v:6.1f}' for k, v in zip(sig, n)), flush=True)
