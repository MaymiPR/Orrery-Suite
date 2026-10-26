"""First look at every module of the reference on its own (the others off), as a map for the detailed measurements:
the small-signal response (pink noise at -50 dBFS, transfer function by cross-spectrum), the gain and harmonic
distortion of 100 Hz and 1 kHz tones from -50 to +6 dBFS, and whether the two channels are processed alike.
    python survey.py            (writes measurements/scorpius/survey.json and prints a summary)"""
import json, os
import numpy as np
import orion as o

OUT = os.path.join(o.ROOT, 'measurements', 'scorpius'); os.makedirs(OUT, exist_ok=True)
CASES = {
    'drive a': dict(params=o.ONLY('drive', hipass=0)),
    'drive b': dict(params=o.ONLY('drive', hipass=0), hidden={'drive.driveMode': 'b'}),
    'drive c': dict(params=o.ONLY('drive', hipass=0), hidden={'drive.driveMode': 'c'}),
    'drive a hp': dict(params=o.ONLY('drive', hipass=1)),
    'harm 80': dict(params={**o.ONLY('harm'), 'Harm Amount': 1}),
    'harm 50': dict(params={**o.ONLY('harm'), 'Harm Amount': 1, 'Harm Freq': 50}),
    'comp b': dict(params={**o.ONLY('comp'), 'Comp Amount': 1}),
    'comp a': dict(params={**o.ONLY('comp'), 'Comp Amount': 1}, hidden={'ids.compression.mode': 'Mode A'}),
    'lift': dict(params={**o.ONLY('lift'), 'Lift Amount': 1}),
    'eq flat': dict(params=o.ONLY('eq')),
    'sub': dict(params={**o.ONLY('sub'), 'Par Sub Level': 1}),
    'low': dict(params={**o.ONLY('low'), 'Par Low Level': 1}),
    'loud': dict(params={**o.ONLY('loud'), 'Par Loud Level': 1}),
    'grit': dict(params={**o.ONLY('grit'), 'Par Grit Level': 1}),
}
LEVELS = [-50, -40, -30, -20, -12, -6, 0, 6]


def response(x, y, fs=o.FS, nper=8192):
    from scipy.signal import csd, welch
    f, pxy = csd(x, y, fs, nperseg=nper); _, pxx = welch(x, fs, nperseg=nper)
    return f, pxy / pxx


if __name__ == '__main__':
    pink = o.noise(4, -50, seed=7, pink=True)
    res = {}
    for name, c in CASES.items():
        jobs = [dict(x=pink, **c)]
        for f in (100, 1000):
            for lv in LEVELS: jobs.append(dict(x=o.tone(f, 1.0, lv), **c))
        ys = o.render_many(jobs, cache=True)
        f, H = response(pink[4800:], ys[0][0, 4800:])
        bands = [31, 50, 80, 125, 200, 500, 1000, 2000, 5000, 10000, 16000]
        resp = {b: float(20 * np.log10(np.abs(H[np.argmin(np.abs(f - b))]))) for b in bands}
        tones = {}
        k = 1
        for fr in (100, 1000):
            for lv in LEVELS:
                y = ys[k][0, 24000:]; k += 1
                h = o.harmonics(y, fr, count=7)
                tones[f'{fr}@{lv}'] = dict(gain=float(h[0] - lv), h2=float(h[1] - h[0]), h3=float(h[2] - h[0]),
                                           h5=float(h[4] - h[0]), lr=float(o.rms_db(ys[k - 1][0] - ys[k - 1][1])))
        res[name] = dict(response=resp, tones=tones)
        print(f'{name:11s} small-signal ' + ' '.join(f'{b}:{v:+.1f}' for b, v in resp.items()))
        for fr in (100, 1000):
            print(f'{"":11s} {fr} Hz gain ' + ' '.join(f'{lv}:{tones[f"{fr}@{lv}"]["gain"]:+.1f}' for lv in LEVELS))
            print(f'{"":11s} {fr} Hz h2/h3 ' + ' '.join(f'{lv}:{tones[f"{fr}@{lv}"]["h2"]:.0f}/{tones[f"{fr}@{lv}"]["h3"]:.0f}' for lv in LEVELS))
    json.dump(res, open(os.path.join(OUT, 'survey.json'), 'w'), indent=1)
