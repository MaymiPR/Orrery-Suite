"""Training material for the learned modules (drive per colour and hi-pass setting, lift): drums, excerpts of the local
demo track, pink noise and log sweeps at many levels, rendered through the reference with only that module on.
Saved (local, git-ignored) to measurements/scorpius/nn/<case>.npz: x (input, mono), y (output), and which pieces
are held out for validation.
    python capture_nn.py [case ...]      cases: drive_a drive_b drive_c drive_a_hp ... lift"""
import os, sys
import numpy as np
import orion as o
import nullmod as N
from order import drums

OUT = os.path.join(o.ROOT, 'measurements', 'scorpius', 'nn'); os.makedirs(OUT, exist_ok=True)
FS = 48000


def pieces():
    out = []
    rng = np.random.default_rng(7)
    if os.path.exists(N.TRACK):
        import scipy.io.wavfile as wf, scipy.signal as sg
        fs, a = wf.read(N.TRACK)
        a = a.astype(np.float64) / (32768.0 if a.dtype == np.int16 else 1.0)
        m = sg.resample_poly(a.mean(1), 160, 147)
        for start in (5, 14, 22, 31, 40, 48, 55):
            seg = m[int(start * FS):int((start + 6) * FS)]
            for db in (0, -6, -14):
                out.append(('track', seg * 10 ** (db / 20)))
    for seed, db in ((5, 0), (6, -6), (7, -12), (8, -20), (9, 3)):
        out.append(('drums', drums(4.0, seed) * 10 ** (db / 20) * 2))
    for db in (-36, -24, -16, -10, -6, -2, 2, 6):
        out.append(('noise', o.noise(1.5, db, seed=int(rng.integers(1000)), pink=True)))
    t = np.arange(4 * FS) / FS
    sweep = np.sin(2 * np.pi * 20 * 4 / np.log(1000) * (np.exp(t / 4 * np.log(1000)) - 1))
    for db in (-20, -6, 0, 6):
        out.append(('sweep', sweep * 10 ** (db / 20)))
    return [(k, np.clip(v, -4, 4).astype(np.float32)) for k, v in out]


CASES = {f'drive_{c}{"_hp" if hp else ""}': dict(params=o.ONLY('drive', hipass=hp), hidden={'drive.driveMode': c}) for c in 'abc' for hp in (0, 1)}
CASES['lift'] = dict(params={**o.ONLY('lift'), 'Lift Amount': 1})

if __name__ == '__main__':
    P = pieces()
    for case in sys.argv[1:] or list(CASES):
        ys = o.render_many([dict(x=x, **CASES[case]) for _, x in P], cache=True)
        np.savez_compressed(os.path.join(OUT, case + '.npz'), x=np.array([x for _, x in P], dtype=object),
                            y=np.array([y[0] for y in ys], dtype=object), kind=np.array([k for k, _ in P]),
                            valid=np.array([i % 7 == 3 for i in range(len(P))]))
        print(case, len(P), 'pieces', sum(len(x) for _, x in P) / FS, 's', flush=True)
