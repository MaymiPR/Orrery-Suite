"""The small-signal (linear) part of every module of the reference: impulse responses by deconvolution of 20 s of white
noise at -50 dBFS (one FFT over the whole render, so long responses are resolved), saved to
measurements/scorpius/ir/<case>.npy (2, 16384) and summarised: the gain at chosen frequencies, the delay of the
peak, whether the response is minimum phase.
    python linear.py [case ...]"""
import os, sys
import numpy as np
import orion as o

IR = os.path.join(o.ROOT, 'measurements', 'scorpius', 'ir'); os.makedirs(IR, exist_ok=True)
N = 16384
CASES = {}
for col in 'abc':
    for hp in (0, 1):
        CASES[f'drive_{col}_hp{hp}'] = dict(params=o.ONLY('drive', hipass=hp), hidden={'drive.driveMode': col})
for fr in (50, 80):
    for amt in (1.0, 0.5):
        CASES[f'harm_{fr}_{amt}'] = dict(params={**o.ONLY('harm'), 'Harm Freq': fr, 'Harm Amount': amt})
for mode in ('Mode A', 'Mode B'):
    for amt in (1.0, 0.5, 0.0):
        CASES[f'comp_{mode[-1].lower()}_{amt}'] = dict(params={**o.ONLY('comp'), 'Comp Amount': amt}, hidden={'ids.compression.mode': mode})
for amt in (1.0, 0.5, 0.0):
    CASES[f'lift_{amt}'] = dict(params={**o.ONLY('lift'), 'Lift Amount': amt})
for band in ('40Hz', '100Hz', '1kHz', '3kHz', '10kHz'):
    for g in (6, 3):
        CASES[f'eq_{band}_{g}'] = dict(params={**o.ONLY('eq'), f'EQ {band}': g})
for p in ('Sub', 'Low', 'Loud', 'Grit'):
    for lv in (1.0, 0.5):
        CASES[f'par_{p.lower()}_{lv}'] = dict(params={**o.ONLY(p.lower()), f'Par {p} Level': lv})
FREQS = [20, 30, 40, 50, 60, 80, 100, 125, 150, 200, 300, 500, 700, 1000, 1500, 2000, 3000, 5000, 7000, 10000, 15000, 20000]


def measure(names):
    x = o.noise(20, -50, seed=21)
    ys = o.render_many([dict(x=x, **CASES[n]) for n in names], cache=True)
    out = {}
    for n, y in zip(names, ys):
        X = np.fft.rfft(x[o.SKIP:]); hs = []
        for c in range(2):
            Y = np.fft.rfft(y[c, o.SKIP:])
            h = np.fft.irfft(Y / X)[:N]
            hs.append(h)
        h = np.array(hs, np.float32)
        np.save(os.path.join(IR, n + '.npy'), h)
        out[n] = h
    return out


def summary(n, h):
    H = np.fft.rfft(h[0], 1 << 18); f = np.fft.rfftfreq(1 << 18, 1 / o.FS)
    mag = [20 * np.log10(abs(H[np.argmin(abs(f - p))]) + 1e-12) for p in FREQS]
    peak = int(np.argmax(np.abs(h[0])))
    lr = 20 * np.log10(np.sqrt(np.mean((h[0] - h[1]) ** 2)) / np.sqrt(np.mean(h[0] ** 2)) + 1e-15)
    return f'{n:16s} peak@{peak:4d} L-R {lr:5.0f} | ' + ' '.join(f'{p}:{m:+.1f}' for p, m in zip(FREQS, mag))


if __name__ == '__main__':
    names = sys.argv[1:] or list(CASES)
    res = measure(names)
    for n in names: print(summary(n, res[n]))
