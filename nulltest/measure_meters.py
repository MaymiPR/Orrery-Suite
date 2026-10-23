"""Gain-reduction meters of the reference, from a RefCapture film (default settings, 100 % UI scale) of
reference/gr_meter_test.f32 (tone bursts per band, see gr_meter_test.json):
  RefCapture "<TGP.vst3>" "The God Particle" film.raw in=reference/gr_meter_test.f32
  python measure_meters.py film.raw
Counts the lit bars of the three band meters per frame (33 bars, 1 px wide on a 4 px pitch, growing left from
x 700 at y 86 / 151 / 216). The numeric readouts beside them hold the deepest GR since the start (read from the
frames by eye: see MEASUREMENTS.md). Writes measurements/meters/gr_bars.json and a plot."""
import json, os, sys
import numpy as np
import tgp, plotstyle as ps

OUT = os.path.join(tgp.ROOT, 'measurements', 'meters'); os.makedirs(OUT, exist_ok=True)
W, H = 888, 359
COLS = np.array([572 + 4 * k for k in range(33)])
ROWS = {'high': 90, 'mid': 155, 'low': 220}

if __name__ == '__main__':
    film = sys.argv[1]
    fsz = 8 + W * H * 3
    mm = np.memmap(film, dtype=np.uint8, mode='r'); n = len(mm) // fsz
    ts = np.array([np.frombuffer(mm[k * fsz:k * fsz + 8].tobytes(), np.float64)[0] for k in range(n)])
    lit = {b: [] for b in ROWS}
    for i in range(n):
        f = np.asarray(mm[i * fsz + 8:(i + 1) * fsz]).reshape(H, W, 3)
        for b, y in ROWS.items():
            on = f[y, COLS, 1] > 110                         # mint #8EDBC4 bars on black
            lit[b].append(dict(count=int(on.sum()), deepest=int(33 - np.argmax(on)) if on.any() else 0))
    seg = json.load(open(os.path.join(tgp.ROOT, 'reference', 'gr_meter_test.json')))
    res = dict(times_s=ts.round(4).tolist(), segments=seg,
               bars={b: [v['count'] for v in lit[b]] for b in ROWS}, deepest_bar={b: [v['deepest'] for v in lit[b]] for b in ROWS})
    json.dump(res, open(os.path.join(OUT, 'gr_bars.json'), 'w'))
    fg, ax = ps.fig('Reference GR meters: lit bars vs time (tone bursts, see gr_meter_test.json)', 's', 'bars lit (of 33)', size=(11, 4.5))
    for b in ROWS:
        ax.plot(ts, res['bars'][b], label=b, lw=1.3, drawstyle='steps-post')
    t = 0
    for s, f, db in seg:
        if db > -40: ax.axvspan(t, t + s, color='#e4e3df', alpha=0.5, lw=0); ax.text(t + 0.05, 34, f'{f} Hz {db:+d}', fontsize=7, color=ps.INK2)
        t += s
    ax.set_ylim(0, 36)
    ps.save(fg, os.path.join(OUT, 'gr_bars.png'))
    print('frames', n)
