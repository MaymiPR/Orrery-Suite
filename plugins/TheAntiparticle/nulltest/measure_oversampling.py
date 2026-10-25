"""Oversampling (the reference's global setting 1 / 2 / 4 / 8x, set by clicking its settings page with
tools/RefCapture): latency, the core path's response, the EQ high shelf near Nyquist, the character section's
small-signal response, aliasing of the character's harmonics and its static gain. Restores 1x (the default) at the
end. Writes measurements/oversampling/oversampling.json and plots."""
import json, os, subprocess, sys
import numpy as np
import tgp, plotstyle as ps

OUT = os.path.join(tgp.ROOT, 'measurements', 'oversampling'); os.makedirs(OUT, exist_ok=True)
FS = tgp.FS
PRE = 2 * FS; N = 32768
CAP = os.path.join(tgp.ROOT, 'build', 'RefCapture_artefacts', 'Release', 'RefCapture.exe')
BUTTON_X = {1: 474.9, 2: 516.8, 4: 558.7, 8: 600.5}      # oversampling buttons on the settings page (design px, y 113)


def set_factor(k):
    shot = os.path.join(tgp.WORK, f'os_{k}.raw'); os.makedirs(tgp.WORK, exist_ok=True)
    path, name = tgp.PLUGINS['ref']
    r = subprocess.run([CAP, path, name, shot, 'design=888', 'drag=874,15.5,0,0,click', f'drag={BUTTON_X[k]},113,0,0,click'],
                       capture_output=True, text=True, timeout=120)
    if 'clicked' not in r.stdout: raise RuntimeError('settings click failed:\n' + r.stdout + r.stderr)


def lockin(y, f, a, b):
    w = np.hanning(b - a); t = np.arange(a, b) / FS
    return abs(np.sum(y[a:b] * w * np.exp(-2j * np.pi * f * t)) / np.sum(w) * 2)


def jobs_for(k):
    imp = np.zeros(PRE + N, np.float32); imp[PRE] = 10 ** (-60 / 20)
    s = f'os{k}'
    hot = np.concatenate([np.zeros(PRE), tgp.tone(9000, 3, -3)])
    mid = np.concatenate([np.zeros(PRE), tgp.tone(1000, 3, -6)])
    return [dict(x=imp, norm=tgp.NONE, raw=True, salt=s),
            dict(x=imp, norm=tgp.EQ_ONLY, params={'EQ High Gain': 6}, raw=True, salt=s),
            dict(x=imp, norm=tgp.CHAR_ONLY, raw=True, salt=s),
            dict(x=hot, norm={**tgp.CHAR_ONLY, 'Character': 1.0}, raw=True, salt=s),
            dict(x=mid, norm=tgp.CHAR_ONLY, raw=True, salt=s),
            dict(x=imp, norm={}, raw=True, salt=s)]


if __name__ == '__main__':
    factors = [1, 2, 4, 8]
    res, H = {}, {}
    try:
        for k in factors:
            set_factor(k)
            lat = tgp.latency()
            ys = tgp.render_many(jobs_for(k), cache=True)
            f = np.fft.rfftfreq(N, 1 / FS)
            hn = ys[0][0, PRE:PRE + N]; Hn = np.fft.rfft(hn)
            He = np.fft.rfft(ys[1][0, PRE:PRE + N]) / Hn; Hc = np.fft.rfft(ys[2][0, PRE:PRE + N]) / Hn
            Hfull = np.fft.rfft(ys[5][0, PRE:PRE + N])
            y = ys[3][0]; a, b = PRE + FS, PRE + 3 * FS
            sp = np.abs(np.fft.rfft(y[a:b] * np.hanning(b - a))); fr = np.fft.rfftfreq(b - a, 1 / FS)
            fund = sp[np.argmin(np.abs(fr - 9000))]
            alias = {str(q): round(float(20 * np.log10(sp[np.argmin(np.abs(fr - q))] / fund + 1e-12)), 1) for q in (3000, 6000, 12000, 15000, 18000, 21000)}
            gmid = 20 * np.log10(lockin(ys[4][0], 1000, PRE + FS, PRE + 3 * FS)) + 6
            peak_none = int(np.argmax(np.abs(hn))); peak_full = int(np.argmax(np.abs(ys[5][0, PRE:PRE + N])))
            mag = lambda X, q: round(float(np.interp(q, f, 20 * np.log10(np.abs(X)))), 3)
            res[f'{k}x'] = dict(reported_latency=lat, ir_peak_bypassed_path=peak_none, ir_peak_all_on=peak_full,
                                bypassed_path_dB={str(q): mag(Hn * 1000, q) for q in (1000, 10000, 16000, 18000, 20000, 21000, 22000, 23000)},
                                eq_high_plus6_dB={str(q): mag(He, q) for q in (5000, 10000, 15000, 18000, 20000, 22000)},
                                character_100_small_dB={str(q): mag(Hc, q) for q in (100, 1000, 5000, 10000, 15000, 20000)},
                                character_200_9k_minus3_aliases_dBc=alias,
                                character_100_1k_minus6_gain_dB=round(float(gmid), 3))
            H[k] = (f, Hn * 1000, He, Hc)
            print(k, json.dumps(res[f'{k}x']))
    finally:
        set_factor(1)
    print('restored 1x; latency', tgp.latency())
    json.dump(res, open(os.path.join(OUT, 'oversampling.json'), 'w'), indent=1)
    fg, ax = ps.fig('Bypassed path (all stages off): magnitude vs oversampling', 'Hz', 'dB', logx=True)
    for k in factors:
        f, Hn, He, Hc = H[k]; ax.plot(f[1:], 20 * np.log10(np.abs(Hn[1:])), label=f'{k}x')
    ax.set_xlim(1000, 24000); ax.set_ylim(-6, 1)
    ps.save(fg, os.path.join(OUT, 'bypassed_path.png'))
    fg, ax = ps.fig('Character 100 %, small signal, vs oversampling', 'Hz', 'dB', logx=True)
    for k in factors:
        f, Hn, He, Hc = H[k]; ax.plot(f[1:], 20 * np.log10(np.abs(Hc[1:])), label=f'{k}x')
    ax.set_xlim(20, 24000)
    ps.save(fg, os.path.join(OUT, 'character_small.png'))
