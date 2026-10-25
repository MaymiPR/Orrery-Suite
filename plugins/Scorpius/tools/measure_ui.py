"""Geometry of Orion's editor from a 200 % RefCapture screenshot (reference/orion/ui200_*.png), in design pixels
(= 100 %): panel and plate edges, column dividers, mint-filled boxes (power buttons, selected segments), mint
outlines, silver regions (knobs, fader caps) with their centres and radii.
  python measure_ui.py reference/orion/ui200_idle.png"""
import sys
import numpy as np
from PIL import Image
from scipy import ndimage

img = np.asarray(Image.open(sys.argv[1]).convert('RGB')).astype(int)
H, W, _ = img.shape
r, g, b = img[..., 0], img[..., 1], img[..., 2]
S = 2.0     # screenshot pixels per design pixel


def comps(mask, min_px=20):
    lab, n = ndimage.label(mask)
    out = []
    for i, sl in enumerate(ndimage.find_objects(lab)):
        if sl is None: continue
        m = lab[sl] == i + 1
        if m.sum() < min_px: continue
        ys, xs = np.nonzero(m)
        out.append(dict(x0=sl[1].start / S, x1=sl[1].stop / S, y0=sl[0].start / S, y1=sl[0].stop / S,
                        cx=(xs.mean() + sl[1].start + 0.5) / S, cy=(ys.mean() + sl[0].start + 0.5) / S, n=int(m.sum())))
    return sorted(out, key=lambda c: (round(c['y0'] / 10), c['x0']))


def show(title, cs):
    print(title)
    for c in cs:
        print(f"   x {c['x0']:7.2f} .. {c['x1']:7.2f}  y {c['y0']:7.2f} .. {c['y1']:7.2f}   w {c['x1']-c['x0']:6.2f} h {c['y1']-c['y0']:6.2f}"
              f"   centre {c['cx']:7.2f}, {c['cy']:7.2f}  px {c['n']}")


mint = (abs(r - 0x8e) < 30) & (abs(g - 0xdb) < 30) & (abs(b - 0xc4) < 30)
show('mint regions (filled boxes and strokes):', comps(mint, 200))
grey = (abs(r - g) < 10) & (abs(g - b) < 12)
silver = grey & (r > 95)
inpanel = np.zeros_like(silver); inpanel[60:1220, :] = True
show('silver regions (knobs, fader caps, outline):', comps(silver & inpanel, 400))
# column dividers: grey vertical lines inside the panel at y = 400 (screenshot)
row = img[400]
xs = [x for x in range(W) if 40 < row[x, 0] < 90 and abs(row[x, 0] - row[x, 1]) < 6 and abs(row[x, 1] - row[x, 2]) < 6]
print('divider candidates at y 200 (design):', sorted(set(round(x / S, 1) for x in xs)))
# horizontal edges along x = 900 (screenshot): where the brightness changes a lot
col = img[:, 900].sum(axis=1)
edges = [y for y in range(1, H) if abs(int(col[y]) - int(col[y - 1])) > 120]
print('brightness edges at x 450 (design):', [round(y / S, 1) for y in edges])
