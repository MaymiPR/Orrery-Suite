"""Plots for the character section from the JSON files the measure_char_* scripts write (measurements/character/)."""
import json, os
import numpy as np
import tgp, plotstyle as ps

D = os.path.join(tgp.ROOT, 'measurements', 'character')
st = json.load(open(os.path.join(D, 'static.json')))
dy = json.load(open(os.path.join(D, 'dyn.json')))
FREQS = sorted({int(k.split('|')[0]) for k in st})
AMOUNTS = [0.0, 0.25, 0.5, 0.75, 1.0]

for fq in FREQS:
    fg, ax = ps.fig(f'Character, {fq} Hz sine: steady-state gain vs input level', 'input dBFS', 'gain dB')
    for am in AMOUNTS:
        r = st[f'{fq}|{am}|up']; ax.plot(r['levels'], r['gain_dB'], label=f'{am * 200:.0f} %')
    r = st[f'{fq}|0.5|down']; ax.plot(r['levels'], r['gain_dB'], ls=':', color=ps.SERIES[2], lw=1.2, label='100 %, falling levels')
    ax.set_ylim(-10, 7)
    ps.save(fg, os.path.join(D, f'static_{fq}hz.png'))

fg, ax = ps.fig('Character, 3rd harmonic vs input level (amount 100 %)', 'input dBFS', 'H3 dBc')
for fq in (40, 100, 300, 1000, 3000):
    r = st[f'{fq}|0.5|up']; ax.plot(r['levels'], [h[1] for h in r['harmonics_dBc']], label=f'{fq} Hz')
ax.set_ylim(-90, 0)
ps.save(fg, os.path.join(D, 'h3_vs_level.png'))

b = dy['bands']
fg, ax = ps.fig('Character bands: -60 dBFS probe gain with a -3 dBFS masker (amount 100 %)', 'probe Hz', 'dB re probes alone', logx=True)
for m, g in b['probe_gain_re_alone_dB'].items():
    ax.plot(b['probes_hz'], g, label=f'masker {m} Hz', lw=1.5)
ps.save(fg, os.path.join(D, 'bands_masker.png'))

for fq in (100, 1000, 8000):
    fg, ax = ps.fig(f'Character, {fq} Hz burst: gain of the fundamental (amount 100 %)', 'ms (step at 2000)', 'dB', size=(10, 4.5))
    for hi in (-20, -6, 0):
        c = np.array(dy['envelope'][f'{fq}|{hi}']['curve_ms_dB']); ax.plot(np.arange(len(c)), c, label=f'-40 -> {hi} -> -40 dBFS', lw=1.3)
    ax.set_xlim(1800, 6000); ax.set_ylim(-7, 5)
    ps.save(fg, os.path.join(D, f'envelope_{fq}hz.png'))
print('ok')
