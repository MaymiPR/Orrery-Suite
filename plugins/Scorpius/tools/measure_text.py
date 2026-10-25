"""Words of text in a 200 % Orion screenshot: light pixels grouped into words (dilated sideways), with their box in
design pixels; the cap height tells the font size, the box bottom the baseline (for words without descenders).
  python measure_text.py reference/orion/ui200_idle.png [y0 y1]   (design-pixel band to look in)"""
import sys
import numpy as np
from PIL import Image
from scipy import ndimage

img = np.asarray(Image.open(sys.argv[1]).convert('RGB')).astype(int)
y0, y1 = (float(sys.argv[2]), float(sys.argv[3])) if len(sys.argv) > 3 else (0, 999)
lum = img.max(axis=2)
# text: bright pixels, but not the big filled boxes (rows of > 60 bright pixels in a 72 px box are fills)
mask = lum > 110
mask[: int(y0 * 2)] = False; mask[int(y1 * 2):] = False
lab, n = ndimage.label(ndimage.binary_dilation(mask, np.ones((3, 9), bool)))
for i, sl in enumerate(ndimage.find_objects(lab)):
    m = (lab[sl] == i + 1) & mask[sl]
    ys, xs = np.nonzero(m)
    if len(ys) < 15 or len(ys) > 4000: continue
    h, w = (ys.max() - ys.min() + 1) / 2, (xs.max() - xs.min() + 1) / 2
    if h > 40 or w > 200: continue
    x0, x1 = (sl[1].start + xs.min()) / 2, (sl[1].start + xs.max() + 1) / 2
    t0, t1 = (sl[0].start + ys.min()) / 2, (sl[0].start + ys.max() + 1) / 2
    c = img[sl][m].mean(axis=0).astype(int)
    print(f'x {x0:7.2f} .. {x1:7.2f} (centre {0.5*(x0+x1):7.2f})  y {t0:7.2f} .. {t1:7.2f} (h {h:5.2f})  colour #{c[0]:02x}{c[1]:02x}{c[2]:02x}')
