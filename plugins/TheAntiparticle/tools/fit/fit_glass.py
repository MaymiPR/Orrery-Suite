"""Measures the glass racetrack of the reference's visualiser as a field over racetrack coordinates and writes it as a
PNG (x = station, y = radius, RGB) that the plug-in embeds and interpolates smoothly (Catmull-Rom).

Coordinates: t in [0, 4) around the track (0..1 top straight left->right, 1..2 right end 0..180 deg, 2..3 bottom
straight right->left, 3..4 left end 180..360 deg; angles from up, clockwise) and r = distance from the centre segment
in design px. Stations: 25 per straight (6 px apart) and 144 per end (1.25 deg); radius r0..r1 in 0.5 px steps (the
pill on top is drawn separately). Fine enough that the reflections' slanted edges stay smooth.

    python plugins/TheAntiparticle/tools/fit/fit_glass.py reference/shots/bypassed.png plugins/TheAntiparticle/resources/glass/glass_off.png
    python plugins/TheAntiparticle/tools/fit/fit_glass.py reference/shots/vis_on_median200.png plugins/TheAntiparticle/resources/glass/glass_lit.png
(vis_on_median200.png: per-pixel median of the active frames of a RefCapture film, which removes the sparks.)"""
import sys
import numpy as np
from PIL import Image

CX, CY, HL = 367.75, 148.25, 75.25
R0, R1, RSTEP = 52.0, 114.0, 0.5
SEG = [25, 144, 25, 144]
SUB = 3

src, out = sys.argv[1], sys.argv[2]
img = np.asarray(Image.open(src).convert('RGB')).astype(float)


def point(t, r):
    seg = int(np.floor(t)) % 4
    f = t - np.floor(t)
    if seg == 0: return CX - HL + 2 * HL * f, CY - r
    if seg == 2: return CX + HL - 2 * HL * f, CY + r
    a = np.pi * f + (0 if seg == 1 else np.pi)
    ex = CX + HL if seg == 1 else CX - HL
    return ex + r * np.sin(a), CY - r * np.cos(a)


def sample(x, y):
    X, Y = 2 * x - 0.5, 2 * y - 0.5            # bilinear at 200 %
    x0, y0 = int(np.floor(X)), int(np.floor(Y))
    fx, fy = X - x0, Y - y0
    p = img[y0:y0 + 2, x0:x0 + 2]
    return p[0, 0] * (1 - fx) * (1 - fy) + p[0, 1] * fx * (1 - fy) + p[1, 0] * (1 - fx) * fy + p[1, 1] * fx * fy


stations = []
for seg, n in enumerate(SEG):
    for i in range(n): stations.append((seg + i / n, seg + (i + 1) / n))
radii = np.arange(R0, R1 + 1e-6, RSTEP)
field = np.zeros((len(radii), len(stations), 3))
for i, (t0, t1) in enumerate(stations):
    for j, r in enumerate(radii):
        acc = np.zeros(3)
        for s in range(SUB):
            acc += sample(*point(t0 + (t1 - t0) * (s + 0.5) / SUB, r))
        field[j, i] = acc / SUB
Image.fromarray(np.clip(np.round(field), 0, 255).astype(np.uint8)).save(out)
print(out, field.shape, 'max', field.max().round())
