"""Gain-reduction meters and their targets in a RefCapture film at 100 %: per frame and band the number of lit bars
(33 bars, 1 px wide on a 4 px pitch growing left from x 700 at y 86 / 151 / 216; deepest = leftmost lit) and the fill
of the target (green channel at the middle of its upper half). Input target: fill of its centre block.

    python grtarget.py film.raw -> measurements/meters/grtarget_<name>.npz"""
import os, sys
import numpy as np
from filmtools import Film

METER_Y = (86, 151, 216)
TARGET_X = (681, 621, 653)
COLS = 700 - 4 * np.arange(33)


def read(path):
    f = Film(path)
    bars = np.zeros((f.n, 3), int); fill = np.zeros((f.n, 3)); infill = np.zeros(f.n)
    for i in range(f.n):
        fr = f.frame(i).astype(int)
        for b in range(3):
            on = fr[METER_Y[b] + 4, COLS, 1] > 110
            bars[i, b] = (np.nonzero(on)[0].max() + 1) if on.any() else 0
            fill[i, b] = fr[METER_Y[b] - 5, TARGET_X[b] + 9, 1]
        infill[i] = fr[113, 47, 1]
    return f.t, bars, fill, infill


if __name__ == '__main__':
    t, bars, fill, infill = read(sys.argv[1])
    name = os.path.basename(sys.argv[1]).replace('.raw', '')
    np.savez(os.path.join(os.path.dirname(sys.argv[1]), f'grtarget_{name}.npz'), t=t, bars=bars, fill=fill, infill=infill)
    for b, nm in enumerate(('high', 'mid', 'low')):
        print(nm)
        for nb in range(0, 34):
            sel = bars[:, b] == nb
            if sel.any(): print(f'   {nb:2d} bars (tip x {700 - 4 * (nb - 1) if nb else "-"}): fill green median {np.median(fill[sel, b]):5.0f}  min {fill[sel, b].min():4.0f} max {fill[sel, b].max():4.0f}  frames {sel.sum()}')
