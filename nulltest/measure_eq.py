"""EQ stage of the reference (character and limiter bypassed): frequency responses per band and knob setting, at two
input levels (is it a dynamic EQ?), and the knob law. Writes measurements/eq/eq.json and plots."""
import json, os
import numpy as np
import tgp, plotstyle as ps

OUT = os.path.join(tgp.ROOT, 'measurements', 'eq'); os.makedirs(OUT, exist_ok=True)
FS = tgp.FS
N = 65536
PRE = 2 * FS                       # start-up: the path's delay settles in ~0.25 s


def impulse(level_db):
    x = np.zeros(PRE + N, np.float32); x[PRE] = 10 ** (level_db / 20); return x


def noise_probe(level_db):
    # white noise, deconvolved: tells whether the response depends on the signal level
    return tgp.noise(3, level_db, seed=3)


def response(y, x0, level_db):
    h = y[0, PRE:PRE + N] / 10 ** (level_db / 20)
    H = np.fft.rfft(h)
    return H


bands = ['EQ Low Gain', 'EQ Mid Gain', 'EQ High Gain']
settings = [-6, -3, 3, 6]
jobs, keys = [], []
for lvl in (-40, -12):
    jobs.append(dict(x=impulse(lvl), norm=tgp.EQ_ONLY, raw=True)); keys.append(('flat', 0, lvl))
    for b in bands:
        for s in settings:
            jobs.append(dict(x=impulse(lvl), norm=tgp.EQ_ONLY, params={b: s}, raw=True)); keys.append((b, s, lvl))
ys = tgp.render_many(jobs)
f = np.fft.rfftfreq(N, 1 / FS)
H = {k: response(y, None, k[2]) for k, y in zip(keys, ys)}
ref = {lvl: H[('flat', 0, lvl)] for lvl in (-40, -12)}
res = {'freq_hz': f[1:2049:4].tolist()}
summary = {}
for b in bands:
    for s in settings:
        for lvl in (-40, -12):
            g = 20 * np.log10(np.abs(H[(b, s, lvl)]) / np.abs(ref[lvl]))
            res[f'{b}|{s}|{lvl}'] = g[1:2049:4].round(3).tolist()
        g40 = 20 * np.log10(np.abs(H[(b, s, -40)]) / np.abs(ref[-40])); g12 = 20 * np.log10(np.abs(H[(b, s, -12)]) / np.abs(ref[-12]))
        sel = (f > 15) & (f < 20000)
        k = np.argmax(np.abs(g40[sel])); fpk = f[sel][k]
        # half-gain frequency(s)
        half = np.where(np.diff(np.sign(np.abs(g40[sel]) - abs(g40[sel][k]) / 2)))[0]
        summary[f'{b} {s:+d}'] = dict(max_dB=round(float(g40[sel][k]), 2), at_hz=round(float(fpk), 1),
                                     half_gain_hz=[round(float(f[sel][i]), 1) for i in half],
                                     at_20Hz=round(float(np.interp(20, f, g40)), 2), at_20kHz=round(float(np.interp(20000, f, g40)), 2),
                                     level_dependence_dB=round(float(np.max(np.abs(g40[sel] - g12[sel]))), 3))
# the bypassed path itself (flat setting vs pure delay)
flat = 20 * np.log10(np.abs(ref[-40]))
res['flat_path_dB'] = flat[1:2049:4].round(3).tolist()
summary['flat path (all knobs 0) deviation from 0 dB, 20 Hz-20 kHz'] = [round(float(flat[(f > 20) & (f < 20000)].min()), 3), round(float(flat[(f > 20) & (f < 20000)].max()), 3)]
json.dump(dict(summary=summary, curves=res), open(os.path.join(OUT, 'eq.json'), 'w'), indent=1)
for k, v in summary.items(): print(k, v)

for b in bands:
    fg, ax = ps.fig(f'{b}: response vs knob (impulse at -40 dBFS, re knob 0)', 'Hz', 'dB', logx=True)
    for s in settings:
        g = 20 * np.log10(np.abs(H[(b, s, -40)]) / np.abs(ref[-40]))
        ax.plot(f[1:], g[1:], label=f'{s:+d} dB')
    ax.set_xlim(10, 24000); ax.set_ylim(-8, 8)
    ps.save(fg, os.path.join(OUT, f'{b.replace(" ", "_").lower()}.png'))
