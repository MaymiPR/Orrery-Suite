"""Where the demo track's residual sits: character only and limiter only, at the track's rate and resampled to
48 kHz; the residual split into the three bands (by FFT masks at the crossovers) and over time.

    python trackdiag.py"""
import numpy as np
from scipy.signal import resample_poly
import tgp, track as T

BANDS = ((0, 157), (157, 2680), (2680, 1e6))


def split(e, fs):
    E = np.fft.rfft(e, axis=1); f = np.fft.rfftfreq(e.shape[1], 1 / fs)
    return [np.sum(np.abs(E[:, (f >= lo) & (f < hi)]) ** 2) for lo, hi in BANDS]


if __name__ == '__main__':
    x0, fs0 = T.load(T.WAV)
    for fs, x in ((fs0, x0), (48000, resample_poly(x0, 160, 147, axis=1).astype(np.float32))):
        pre = 2 * fs
        xp = np.concatenate([np.zeros((2, pre), np.float32), x], axis=1)
        cs = [('character only', tgp.CHAR_ONLY), ('limiter only', tgp.LIM_ONLY), ('defaults', {})]
        jobs = [dict(x=xp, norm=nm) for _, nm in cs]
        ref = tgp.render_many(jobs, 'ref', fs=fs, block=512, cache=True)
        ours = tgp.render_many(jobs, 'model', fs=fs, block=512)
        for (name, _), r, o in zip(cs, ref, ours):
            a, b = r[:, pre:].astype(np.float64), o[:, pre:].astype(np.float64)
            e = a - b
            tot = 10 * np.log10(np.sum(e ** 2) / np.sum(a ** 2))
            eb, ab = split(e, fs), split(a, fs)
            m, s = (e[0] + e[1]) / 2, (e[0] - e[1]) / 2
            print(f'{fs} {name:16s} {tot:7.2f} dB | per band (re band): ' + ' '.join(f'{10 * np.log10(p / q):7.2f}' for p, q in zip(eb, ab))
                  + ' | share of residual: ' + ' '.join(f'{100 * p / sum(eb):4.0f}%' for p in eb)
                  + f' | side share {100 * np.sum(s ** 2) / (np.sum(m ** 2) + np.sum(s ** 2)):.0f}%', flush=True)
            if name == 'character only':
                sec = [10 * np.log10(np.sum(e[:, i:i + fs] ** 2) / np.sum(a[:, i:i + fs] ** 2)) for i in range(0, e.shape[1] - fs, fs)]
                lvl = [tgp.rms_db(a[:, i:i + fs]) for i in range(0, e.shape[1] - fs, fs)]
                print('   per second (res / level): ' + ' '.join(f'{v:.0f}/{l:.0f}' for v, l in zip(sec, lvl)))
        np.save(tgp.ROOT + f'/measurements/character/track_char_{fs}.npy', np.stack([ref[0], ours[0]]))
