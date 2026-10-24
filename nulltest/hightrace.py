"""The high band's gain n(t) read directly: a quiet 16 kHz probe tone (-40 dBFS, inside the high band) rides along with
a loud driver in the high band; the probe's gain at the output, demodulated over 2 ms, is the band's gain (each
driver is rendered with the probe and with it inverted, and half the difference kept, so the driver's own output
cancels; at low amounts the shaper is weak, so its cross-modulation barely touches the probe). Reference against ours.

    python hightrace.py [amount 0..1]"""
import sys
import numpy as np
from scipy.signal import butter, sosfiltfilt
import tgp, ident as I

FS = tgp.FS; PRE = 2 * FS
FP = 16000.0; PROBE = 10 ** (-40 / 20)


def drivers(n, seed=5):
    t = np.arange(n) / FS; rng = np.random.default_rng(seed)
    band = lambda w: sosfiltfilt(butter(6, (5000, 11000), 'band', fs=FS, output='sos'), w)
    noise = band(rng.standard_normal(n)); noise /= np.sqrt(2 * np.mean(noise ** 2))
    gate = np.zeros(n); seg = int(0.25 * FS)
    for a in range(PRE, n, 2 * seg): gate[a:a + seg] = 1.0                    # 250 ms on / off
    clicks = np.zeros(n)
    for a in range(PRE, n, int(0.1 * FS)): clicks[a:a + 48] = rng.standard_normal(48) * np.hanning(48)
    clicks = band(clicks); clicks /= np.max(np.abs(clicks))
    out = {'noise -6': noise * 10 ** (-6 / 20), 'noise bursts -6': noise * gate * 10 ** (-6 / 20),
           'sine 8k -6': np.sin(2 * np.pi * 8000 * t) * 10 ** (-6 / 20),
           'two-tone -6': (np.sin(2 * np.pi * 7000 * t) + np.sin(2 * np.pi * 9100 * t)) / np.sqrt(2) * 10 ** (-6 / 20),
           'clicks 0 dBFS peak': clicks}
    for v in out.values(): v[:PRE // 2] = 0
    return out


def gain_trace(y, n):
    t = np.arange(n) / FS
    z = y * np.exp(-2j * np.pi * FP * t)
    W = int(0.002 * FS)
    return np.abs(np.convolve(z, np.ones(W) / W, 'same')) * 2 / PROBE


if __name__ == '__main__':
    amt = float(sys.argv[1]) if len(sys.argv) > 1 else 0.125
    n = PRE + 3 * FS; t = np.arange(n) / FS
    probe = PROBE * np.sin(2 * np.pi * FP * t); probe[:PRE // 2] = 0
    dr = drivers(n)
    M = 10 ** (I.GAINS[2][int(round(amt * 8))] / 20) if amt * 8 == int(amt * 8) else 1.0
    # each driver with the probe and with the probe inverted: half the difference is the probe through the band's
    # gain (everything even in the probe, the driver and its own distortion, cancels)
    jobs = []
    for v in list(dr.values()) + [0 * probe]:
        for sgn in (1, -1): jobs.append(dict(x=(v + sgn * probe).astype(np.float32), norm={**tgp.CHAR_ONLY, 'Character': amt}))
    rr = tgp.render_many(jobs, 'ref', cache=True); oo = tgp.render_many(jobs, 'model')
    r = [0.5 * (rr[2 * i] - rr[2 * i + 1]) for i in range(len(jobs) // 2)]
    o = [0.5 * (oo[2 * i] - oo[2 * i + 1]) for i in range(len(jobs) // 2)]
    g0r, g0o = gain_trace(r[-1][0], n), gain_trace(o[-1][0], n)
    base_r, base_o = np.median(g0r[PRE + FS:]), np.median(g0o[PRE + FS:])
    for (name, _), yr, yo in zip(dr.items(), r, o):
        gr = 20 * np.log10(gain_trace(yr[0], n) / base_r); go = 20 * np.log10(gain_trace(yo[0], n) / base_o)
        seg = slice(PRE + FS, PRE + 2 * FS)
        print(f'{name:20s} gain dB: ref mean {np.mean(gr[seg]):+.3f} (min {np.min(gr[seg]):+.3f}, max {np.max(gr[seg]):+.3f})   '
              f'ours mean {np.mean(go[seg]):+.3f} (min {np.min(go[seg]):+.3f}, max {np.max(go[seg]):+.3f})', flush=True)
        np.save(f'{tgp.ROOT}/measurements/character/hightrace_{name.split()[0]}_{name.split()[1] if len(name.split()) > 2 else ""}_{amt}.npy', np.stack([gr, go]))
