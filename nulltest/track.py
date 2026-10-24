"""Null tests on a real mix: a demo track (reference/demo_future_house.wav, git-ignored: it is not ours to publish)
through the reference and our built VST3 at the track's own rate, block 512, with the stage switches as a user flips
them in a host: each stage bypassed in turn, and the limiter switched off and on while playing.

    python track.py [wav] [--model] [--vary]  (--model: our processor via ApRender instead of the VST3; --vary: irregular
                                               host blocks, as FL Studio sends them)
Writes nulltest/results_track.json."""
import json, os, sys, wave
import numpy as np
import tgp

WAV = os.path.join(tgp.ROOT, 'reference', 'demo_future_house.wav')
BLOCK = 512


def load(path):
    w = wave.open(path)
    assert w.getsampwidth() == 2, 'expects 16-bit PCM'
    x = np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, w.getnchannels()).T / 32768.0
    if x.shape[0] == 1: x = np.vstack([x, x])
    return x[:2].astype(np.float32), w.getframerate()


def cases(fs):
    on = lambda t: int(t * fs) // BLOCK * BLOCK
    return [
        ('defaults', {}, {}, None),
        ('limiter bypassed', {'Limiter Bypass': 1}, {}, None),
        ('character bypassed', {'Character Bypass': 1}, {}, None),
        ('eq bypassed', {'EQ Bypass': 1}, {}, None),
        ('all stages bypassed', tgp.NONE, {}, None),
        ('limiter only', tgp.LIM_ONLY, {}, None),
        ('character only', tgp.CHAR_ONLY, {}, None),
        ('character 200 %', {'Character': 1.0}, {}, None),
        ('limiter gain 12', {}, {'Limiter Input Gain': 12}, None),
        ('limiter switched off at 20 s, on at 40 s', {}, {}, [('Limiter Bypass', on(20), 1), ('Limiter Bypass', on(40), 0)]),
        ('character switched off at 20 s, on at 40 s', {}, {}, [('Character Bypass', on(20), 1), ('Character Bypass', on(40), 0)]),
    ]


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    x, fs = load(args[0] if args else WAV)
    pre = 2 * fs                                    # the reference's start-up ramp, as in nulltest.py
    xp = np.concatenate([np.zeros((2, pre), np.float32), x], axis=1)
    cs = cases(fs)
    jobs = [dict(x=xp, norm=nm, params=pm, auto=[(k, s + pre, v) for k, s, v in au] if au else None) for _, nm, pm, au in cs]
    vary = '--vary' in sys.argv
    ref = tgp.render_many(jobs, 'ref', fs=fs, block=BLOCK, cache=True, vary=vary)
    ours = tgp.render_many(jobs, 'model' if '--model' in sys.argv else 'ours', fs=fs, block=BLOCK, vary=vary)
    res = {}
    for (name, *_), r, o in zip(cs, ref, ours):
        a, b = r[:, pre:].astype(np.float64), o[:, pre:].astype(np.float64)
        d = 10 * np.log10(np.sum((a - b) ** 2) / max(np.sum(a ** 2), 1e-30))
        # the worst second, and the lag (samples) that would null best, in case the stages disagree in timing
        sec = [10 * np.log10(np.sum((a[:, i:i + fs] - b[:, i:i + fs]) ** 2) / max(np.sum(a[:, i:i + fs] ** 2), 1e-30))
               for i in range(0, a.shape[1] - fs, fs)]
        lags = {L: np.sum((a[:, 100:-100] - np.roll(b, L, axis=1)[:, 100:-100]) ** 2) for L in range(-64, 65)}
        best = min(lags, key=lags.get)
        res[name] = dict(residual_dB=round(float(d), 2), worst_second_dB=round(float(max(sec)), 2), worst_at_s=int(np.argmax(sec)),
                         best_lag=int(best))
        print(f'{d:8.2f} dB   worst second {max(sec):7.2f} dB at {np.argmax(sec):3d} s   best lag {best:+d}   {name}', flush=True)
    json.dump(res, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'results_track_vary.json' if vary else 'results_track.json'), 'w'), indent=1)
