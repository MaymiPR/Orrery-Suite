"""Probe signals for identifying the character section's gain law per band (nulltest/ident.py reads the gain n(t)
sample by sample from the reference's output). A tone in the middle of the band:
  static    3 s steps -40 -30 -20 -14 -8 -2 +4 dBFS (the slow part settles), then down again
  bursts    from a -50 dBFS bed to -24 / -12 / 0 dBFS for 20 ms, 200 ms and 2 s, 3 s of bed after each
  twotone   the static steps with two equal tones (band frequency x 0.8 and x 1.25) at the same RMS as the one tone:
            3 dB more peak, and a beating envelope, which tells a mean-square detector from ones that react to peaks
"""
import numpy as np
import tgp

FS = tgp.FS
PRE = 2 * FS
BAND_FREQ = {'low': 70.0, 'mid': 700.0, 'high': 8000.0}
STATIC_LEVELS = [-40, -30, -20, -14, -8, -2, 4, -2, -8, -14, -20, -30, -40]
STATIC_SECS = 3.0
BED = -50
BURSTS = [(lvl, dur) for lvl in (-24, -12, 0) for dur in (0.02, 0.2, 2.0)]
GAP = 3.0


def tone_segments(f, segs):
    """segs: [(seconds, dBFS)], one continuous sine (no phase jumps)"""
    out, t0 = [], 0
    for secs, db in segs:
        n = int(round(secs * FS)); t = (np.arange(n) + t0) / FS; t0 += n
        out.append(10 ** (db / 20) * np.sin(2 * np.pi * f * t))
    return np.concatenate([np.zeros(PRE)] + out).astype(np.float32)


def static_signal(band):
    return tone_segments(BAND_FREQ[band], [(STATIC_SECS, l) for l in STATIC_LEVELS])


def twotone_signal(band):
    f = BAND_FREQ[band]
    a = tone_segments(f * 0.8, [(STATIC_SECS, l) for l in STATIC_LEVELS])
    b = tone_segments(f * 1.25, [(STATIC_SECS, l) for l in STATIC_LEVELS])
    return ((a + b) / np.sqrt(2)).astype(np.float32)


def burst_signal(band):
    segs = [(1.0, BED)]
    for lvl, dur in BURSTS: segs += [(dur, lvl), (GAP, BED)]
    return tone_segments(BAND_FREQ[band], segs)


def burst_starts():
    """sample index (in the full signal, pre-roll included) where each burst starts and ends"""
    t = PRE + FS; out = []
    for lvl, dur in BURSTS:
        n = int(round(dur * FS)); out.append((lvl, dur, t, t + n)); t += n + int(round(GAP * FS))
    return out
