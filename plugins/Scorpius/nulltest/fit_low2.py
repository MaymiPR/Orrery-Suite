"""The low parallel as an octave divider times an envelope (MEASUREMENTS.md 14): a flip-flop toggled at the falling
zero crossings of the input through a 6th-order low-pass (216.7 Hz), its square wave (starting negative) times the
peak envelope of that signal, through a 4th-order Butterworth low-pass (46.4 Hz), then v + a v^2 and a soft limit.

Found from the reference's contribution on steady tones (a square wave at half the frequency, scaled by the tone's
level: not the input times +-1) and a tone sweep 25 ... 160 Hz at -30 dBFS (the octave's level follows a 4th-order
Butterworth at ~47 Hz to 0.5 dB; the detector's low-pass sets its phase). This script checks the sweep and fits the
envelope and the level terms on tones, chords and bursts (pure Python, slow: a few minutes).
    python fit_low2.py"""
import numpy as np
import scipy.signal as sg
from scipy.optimize import minimize
import orion as o

FS = o.FS
P = {**o.NONE, 'Par Low On/Off': 1, 'Par Low Level': 1.0}
DET = sg.butter(6, 216.7, fs=FS, output='sos')
OUT = sg.butter(4, 46.4, fs=FS, output='sos')
t = np.arange(int(2.5 * FS)) / FS


def flip_env(d, ka, kr, h=1e-7):
    s = np.zeros(len(d)); e = np.zeros(len(d))
    state, armed, env = 0.0, False, 0.0
    for i, v in enumerate(d):
        if v > h: armed = True
        if armed and v < -h:
            state = -1.0 if state == 0 else -state; armed = False
        a = abs(v); env = a + (env - a) * (ka if a > env else kr)
        s[i] = state; e[i] = env
    return s, e


def model(x, gain, att, rel, sq, ceil):
    d = sg.sosfilt(DET, x)
    s, e = flip_env(d, np.exp(-1 / (att * 0.001 * FS)), np.exp(-1 / (rel * 0.001 * FS)))
    v = sg.sosfilt(OUT, gain * s * e)
    v = v + sq * v * v
    return ceil * np.tanh(v / ceil)


def contribution(x):
    xs = np.stack([x, x]).astype(np.float32)
    return (o.render(xs, 'ref', cache=True, params=P) - xs)[0].astype(np.float64)


def null(c, m, skip=2400):
    return 10 * np.log10(np.sum((c[skip:] - m[skip:]) ** 2) / np.sum(c[skip:] ** 2))


if __name__ == '__main__':
    tone = lambda f, db: 10 ** (db / 20) * np.sin(2 * np.pi * f * t)
    sigs = {f'{f} Hz {db}': tone(f, db) for f in (40, 80) for db in (-30, -12)}
    sigs['60+90 Hz -15'] = tone(60, -15) + tone(90, -15)
    sigs['40+60+100 Hz -18'] = tone(40, -18) + tone(60, -18) + tone(100, -18)
    C = {k: contribution(x) for k, x in sigs.items()}
    v0 = [0.89365, 0.05347, 49.633, 0.90778, 1.37852]
    f = lambda v: np.mean([null(C[k], model(x, *v)) for k, x in sigs.items()])
    print('start', round(f(v0), 2), 'dB re the contribution')
    r = minimize(f, v0, method='Nelder-Mead', options=dict(maxiter=60))
    print('fit', round(r.fun, 2), 'gain, attack ms, release ms, square, ceiling:', np.round(r.x, 5))
    for k, x in sigs.items():
        print(f'   {k}: {null(C[k], model(x, *r.x)):.1f} dB')
