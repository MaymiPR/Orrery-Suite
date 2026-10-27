"""Training material for the learned modules (drive per colour and hi-pass setting, lift): drums, excerpts of the local
demo track, pink noise and log sweeps at many levels, rendered through the reference with only that module on.
Saved (local, git-ignored) to measurements/scorpius/nn/<case>.npz: x (input, mono), y (output), and which pieces
are held out for validation.
    python capture_nn.py [case ...]      cases: drive_a drive_b drive_c drive_a_hp ... lift harm80 harm50 grit
Grit is captured on the left channel alone (the right silent): it works on each channel by itself, except that on
input whose channels are exactly equal and near full scale it behaves differently (MEASUREMENTS.md 9)."""
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


def synth_mix(secs, seed):
    """a dense synthetic mix: a bass line, chords (saw-like partials), a pink noise bed and drums, varied by the seed"""
    rng = np.random.default_rng(seed)
    t = np.arange(int(secs * FS)) / FS
    root = rng.choice([41.2, 43.7, 49.0, 55.0, 61.7])
    notes = root * 2 ** (rng.choice([0, 3, 5, 7, 10, 12], 4) / 12)
    f = notes[(t / rng.uniform(0.25, 0.75)).astype(int) % 4]
    bass = np.sin(2 * np.pi * np.cumsum(f) / FS) * rng.uniform(0.3, 0.6)
    chord = sum(np.sin(2 * np.pi * root * 4 * 2 ** (iv / 12) * k * t) / k for iv in (0, 4, 7) for k in range(1, 8)) * rng.uniform(0.04, 0.1)
    bed = o.noise(secs, -30 + rng.uniform(-6, 6), seed=int(seed), pink=True)[:len(t)]
    x = bass + chord + bed + drums(secs, int(seed) + 100)[:len(t)] * rng.uniform(0.6, 1.0)
    return x / np.max(np.abs(x))


def loud_pieces():
    """more loud material (drive_a_hp_loud): the track's other passages at 0 / +2 dB, synthetic mixes and drums near
    full scale"""
    out = []
    if os.path.exists(N.TRACK):
        import scipy.io.wavfile as wf, scipy.signal as sg
        fs, a = wf.read(N.TRACK)
        a = a.astype(np.float64) / (32768.0 if a.dtype == np.int16 else 1.0)
        m = sg.resample_poly(a.mean(1), 160, 147)
        for start in np.arange(2.0, 62.0, 2.5):
            seg = m[int(start * FS):int((start + 3) * FS)]
            if len(seg) < 3 * FS: continue
            if any(start < v + 6 and start + 3 > v for v in (14, 31, 48)): continue   # (not the held-out passages)
            for db in (0, 2):
                out.append(('track', seg * 10 ** (db / 20)))
    for seed in range(12):
        x = synth_mix(4.0, seed)
        for db in (-1, 1):
            out.append(('synth', x * 10 ** (db / 20)))
    for seed in range(10, 18):
        out.append(('drums', drums(4.0, seed) * 2))
    return [(k, np.clip(v, -4, 4).astype(np.float32)) for k, v in out]


CASES = {f'drive_{c}{"_hp" if hp else ""}': dict(params=o.ONLY('drive', hipass=hp), hidden={'drive.driveMode': c}) for c in 'abc' for hp in (0, 1)}
CASES['drive_a_hp_loud'] = dict(params=o.ONLY('drive', hipass=1), hidden={'drive.driveMode': 'a'}, pieces=loud_pieces)
CASES['lift'] = dict(params={**o.ONLY('lift'), 'Lift Amount': 1})
CASES['harm80'] = dict(params={**o.ONLY('harm'), 'Harm Amount': 1.0, 'Harm Freq': 80})
CASES['harm50'] = dict(params={**o.ONLY('harm'), 'Harm Amount': 1.0, 'Harm Freq': 50})
for f_ in (80, 50):                  # (harmonics at partial amounts: the amount-conditioned model)
    for a_ in (0.25, 0.5, 0.75):
        CASES[f'harm{f_}_a{int(a_ * 100):03d}'] = dict(params={**o.ONLY('harm'), 'Harm Amount': a_, 'Harm Freq': f_})
CASES['grit'] = dict(params={**o.NONE, 'Par Grit On/Off': 1, 'Par Grit Level': 1.0}, left=True)

if __name__ == '__main__':
    for case in sys.argv[1:] or list(CASES):
        P = CASES[case].get('pieces', pieces)()
        spec = {k: v for k, v in CASES[case].items() if k not in ('left', 'pieces')}
        stereo = (lambda x: np.stack([x, 0 * x])) if CASES[case].get('left') else (lambda x: x)
        ys = o.render_many([dict(x=stereo(x), **spec) for _, x in P], cache=True)
        np.savez_compressed(os.path.join(OUT, case + '.npz'), x=np.array([x for _, x in P], dtype=object),
                            y=np.array([y[0] for y in ys], dtype=object), kind=np.array([k for k, _ in P]),
                            valid=np.array([i % 7 == 3 for i in range(len(P))]))
        print(case, len(P), 'pieces', sum(len(x) for _, x in P) / FS, 's', flush=True)
