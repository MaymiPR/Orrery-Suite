"""Probes for the I/O readouts (peak and RMS numbers above the LED meters), each followed by silence so the decay shows.
  probe 1: tones, noise, short bursts, a level step down and an inter-sample peak
  probe 2: what the RMS readout averages (pulse trains of several densities: |x| or x^2), constant-magnitude tones at
           two levels, DC, the same tone repeated (spread between runs), a slow fade (where it shows -inf), and an
           inter-sample peak first of all, while the peak readout is still empty (sample or true peak)
Writes reference/readout_probe<N>.f32 (mono float32 48 kHz, RefCapture's in=) and .json (segments: label, start, secs).
  python readout_probe.py [1|2]
  RefCapture "<TGP.vst3>" "The God Particle" film.raw in=reference/readout_probe2.f32"""
import json, os, sys
import numpy as np
import tgp

FS = 48000


def tone(secs, db, f=1000.0, phase=0.0):
    t = np.arange(int(secs * FS)) / FS
    return 10 ** (db / 20) * np.sin(2 * np.pi * f * t + phase)


def noise(secs, rms_db, seed=1):
    x = np.random.default_rng(seed).standard_normal(int(secs * FS))
    return x / np.sqrt(np.mean(x ** 2)) * 10 ** (rms_db / 20)


def pulses(secs, db, period):
    """One sample of amplitude db every `period` samples, alternating in sign (no DC): mean |x| = A / period,
    RMS = A / sqrt(period), peak A."""
    x = np.zeros(int(secs * FS))
    k = np.arange(0, len(x), period)
    x[k] = 10 ** (db / 20) * np.where(np.arange(len(k)) % 2 == 0, 1.0, -1.0)
    return x


def silence(secs):
    return np.zeros(int(secs * FS))


PROBES = {
    '1': [
        ('silence', silence(1.0)),
        ('1k -6', tone(2.0, -6)), ('silence', silence(4.0)),
        ('1k -20', tone(2.0, -20)), ('silence', silence(3.0)),
        ('1k -6', tone(2.0, -6)), ('1k -20 after -6', tone(2.0, -20)), ('silence', silence(3.0)),
        ('noise rms -12', noise(2.0, -12)), ('silence', silence(3.0)),
        ('burst 10 ms 1k -6', tone(0.01, -6)), ('silence', silence(3.0)),
        ('burst 100 ms 1k -6', tone(0.1, -6)), ('silence', silence(3.0)),
        ('burst 300 ms 1k -6', tone(0.3, -6)), ('silence', silence(3.0)),
        ('12k -6 at 45 deg (sample peak -9)', tone(2.0, -6, 12000.0, np.pi / 4)), ('silence', silence(3.0)),
        ('100 Hz -6', tone(2.0, -6, 100.0)), ('silence', silence(3.0)),
        ('1k -30', tone(1.0, -30)), ('1k -10 after -30', tone(2.0, -10)), ('silence', silence(3.0)),
    ],
    '2': [
        ('silence', silence(1.0)),
        ('12k -6 at 45 deg (sample peak -9.03, true peak -6.02)', tone(2.5, -6, 12000.0, np.pi / 4)), ('silence', silence(1.5)),
        ('12k -20 at 45 deg (|x| constant -23.03)', tone(2.5, -20, 12000.0, np.pi / 4)), ('silence', silence(1.5)),
        ('pulses -6 every 2', pulses(2.5, -6, 2)), ('pulses -6 every 4', pulses(2.5, -6, 4)),
        ('pulses -6 every 8', pulses(2.5, -6, 8)), ('pulses -6 every 16', pulses(2.5, -6, 16)),
        ('pulses -6 every 64', pulses(2.5, -6, 64)), ('silence', silence(1.5)),
        ('DC -12', np.full(int(2.5 * FS), 10 ** (-12 / 20))), ('silence', silence(1.5)),
        ('1k -6 run 1', tone(2.5, -6)), ('silence', silence(1.0)),
        ('1k -6 run 2', tone(2.5, -6)), ('silence', silence(1.0)),
        ('1k -6 run 3', tone(2.5, -6)), ('silence', silence(1.0)),
        ('997 Hz -6', tone(2.5, -6, 997.0)), ('silence', silence(1.0)),
        ('noise rms -20', noise(2.5, -20, 2)), ('silence', silence(1.5)),
        ('1k fade -60 to -120 dB over 6 s', tone(6.0, 0) * 10 ** (np.linspace(-60, -120, 6 * FS) / 20)),
        ('silence', silence(1.0)),
    ],
}

if __name__ == '__main__':
    which = sys.argv[1] if len(sys.argv) > 1 else '2'
    name = 'readout_probe' if which == '1' else f'readout_probe{which}'
    out, seg, t = [], [], 0.0
    for label, x in PROBES[which]:
        seg.append(dict(label=label, start=round(t, 4), secs=len(x) / FS))
        out.append(x); t += len(x) / FS
    x = np.concatenate(out).astype(np.float32)
    x.tofile(os.path.join(tgp.ROOT, 'reference', name + '.f32'))
    json.dump(seg, open(os.path.join(tgp.ROOT, 'reference', name + '.json'), 'w'), indent=1)
    print(f'{name}: {len(x) / FS:.1f} s, {len(seg)} segments')
