"""Knob mouse behaviour, the reference against ours, with RefCapture drag scripts: parameter values after plain, Ctrl,
Shift (velocity) and overshoot drags, and whether a knob's caption shows its value when the mouse is at points in and
around its cell (hover). Run from PowerShell (the Bash tool cuts GUI processes short).

    python uiknobs.py"""
import os, re, subprocess
import numpy as np
from PIL import Image
import tgp

CAP = os.path.join(tgp.ROOT, 'build', 'RefCapture_artefacts', 'Release', 'RefCapture.exe')
SHOTS = os.path.join(tgp.ROOT, 'reference', 'shots')
OUT = '841.74,300'
DRAGS = [f'{OUT},0,10,,100', f'{OUT},0,10,ctrl,100', f'{OUT},0,30,ctrl+mid,30', f'{OUT},0,-80,back,80']
DRAGS += [f'{OUT},0,{d},shift,{n}' for d, n in ((20, 10), (20, 5), (16, 2), (20, 2), (12, 1), (20, 1), (40, 1), (60, 1), (100, 1), (200, 1))]
HOVER = ['45.755,262', '45.755,268', '45.755,313', '45.755,358', '1,313', '3,313', '88,313', '90,313',
         '133.375,69', '133.375,71', '133.375,134', '133.375,137', '92,90', '176,90']


def run(who, args, tag):
    path, name = tgp.PLUGINS[who]
    r = subprocess.run([CAP, path, name, os.path.join(SHOTS, f'{tag}.png'), 'design=888'] + args, capture_output=True, text=True, timeout=600)
    return r.stdout


def drags(who):
    args = []
    for d in DRAGS: args += [f'drag={d}', f'drag={OUT},0,0,dbl']
    out = run(who, args, f'uik_{who}')
    vals = re.findall(r'^after: .*?\[Output Gain=([-\d.]+)\]', out, re.M)
    return [float(v) for v in vals[0::2]]


def hover(who):
    args = []
    for q in HOVER: args += ['drag=450,150,0,0,hover', f'drag={q},0,0,hover']
    run(who, args, f'uih_{who}')
    res = []
    for i, q in enumerate(HOVER):
        box = (5, 268, 88, 284) if float(q.split(',')[1]) > 200 else (95, 113, 172, 129)
        cut = lambda n: np.asarray(Image.open(os.path.join(SHOTS, f'uih_{who}_{n}_hover.png')).convert('L')).astype(float)[box[1]:box[3], box[0]:box[2]]
        res.append(np.abs(cut(2 * i + 2) - cut(2 * i + 1)).mean() > 2.0)      # differs from the neutral caption
    return res


if __name__ == '__main__':
    r, o = drags('ref'), drags('ours')
    print('drag (output knob from 0 dB)            ref      ours')
    for d, a, b in zip(DRAGS, r, o):
        print(f'  {d:34s} {a:8.3f} {b:8.3f}' + ('' if abs(a - b) < 0.0015 else '   <-- differs'))
    hr, ho = hover('ref'), hover('ours')
    print('hover point            ref      ours')
    for q, a, b in zip(HOVER, hr, ho):
        print(f'  {q:18s} {"value" if a else "caption":8s} {"value" if b else "caption":8s}' + ('' if a == b else '   <-- differs'))
