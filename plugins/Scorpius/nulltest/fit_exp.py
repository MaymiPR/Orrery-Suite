"""The shared expander: threshold, slope, hold and release fitted to the reference's gain over time (1 kHz tones in
steps, harmonics on at amount 0 so only the expander acts), as the gain in dB per 5 ms."""
import numpy as np
from scipy.optimize import minimize
import orion as o

FS = 48000
def steps():
    t = np.arange(4 * FS) / FS; out = []
    for hi, lo in ((-40, -75), (-20, -75), (-40, -65), (-50, -62)):
        lev = np.where(t < 1.0, 10 ** (hi / 20), 10 ** (lo / 20)); out.append((lev * np.sin(2 * np.pi * 1000 * t)).astype(np.float32))
    for lv in (-80, -70, -64, -60, -57, -54):
        out.append(o.tone(1000, 1.5, lv))
    return out
X = steps()
P = {**o.ONLY('harm'), 'Harm Amount': 0}
REF = o.render_many([dict(x=x, params=P) for x in X], cache=True)

def track(y, x):
    return np.array([o.rms_db(y[i:i + 240]) - o.rms_db(x[i:i + 240]) for i in range(0, len(x) - 240, 240)])

RT = [track(r[0], x) for r, x in zip(REF, X)]

def cost(v):
    tune = dict(expThresholdDb=v[0], expSlope=v[1], expHoldMs=v[2], expReleaseMs=v[3])
    ours = o.render_many([dict(x=x, params=P, tune=tune) for x in X], plugin='model')
    e = np.concatenate([np.clip(track(m[0], x), -80, 5) - np.clip(rt, -80, 5) for m, x, rt in zip(ours, X, RT)])
    return float(np.sqrt(np.mean(e ** 2)))

if __name__ == '__main__':
    v0 = [-58.0, 2.0, 1200.0, 120.0]
    print('start', cost(v0))
    s = minimize(cost, v0, method='Nelder-Mead', options=dict(xatol=0.5, fatol=0.01, maxiter=150))
    print('best', np.round(s.x, 3).tolist(), s.fun)
    tune = dict(expThresholdDb=s.x[0], expSlope=s.x[1], expHoldMs=s.x[2], expReleaseMs=s.x[3])
    ours = o.render_many([dict(x=x, params=P, tune=tune) for x in X[:1]], plugin='model')
    print('ref ', ' '.join('%.1f' % v for v in RT[0][200:400:8]))
    print('ours', ' '.join('%.1f' % v for v in track(ours[0][0], X[0])[200:400:8]))
