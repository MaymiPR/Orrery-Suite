"""Visual comparison of our editor snapshot with a reference screenshot (both 200 %):
   python tools/ui_compare.py reference.png ours.png out.png [x0 y0 x1 y1]
Writes reference / ours / |difference| x3 stacked, and prints the mean absolute difference (overall and per area)."""
import sys
import numpy as np
from PIL import Image

ref = np.asarray(Image.open(sys.argv[1]).convert('RGB')).astype(float)
our = np.asarray(Image.open(sys.argv[2]).convert('RGB')).astype(float)
h, w = min(ref.shape[0], our.shape[0]), min(ref.shape[1], our.shape[1])
ref, our = ref[:h, :w], our[:h, :w]
if len(sys.argv) > 7:
    x0, y0, x1, y1 = map(int, sys.argv[4:8])
    ref, our = ref[y0:y1, x0:x1], our[y0:y1, x0:x1]
d = np.abs(ref - our)
print(f'mean |diff| {d.mean():.2f}')
areas = {'top bar': (0, 0, 1776, 61), 'input': (0, 61, 178, 531), 'eq': (178, 61, 352, 531), 'centre': (352, 61, 1118, 531),
         'gain red.': (1118, 61, 1422, 531), 'limiter': (1422, 61, 1596, 531), 'output': (1596, 61, 1776, 531), 'plate': (0, 531, 1776, 719)}
if len(sys.argv) <= 7:
    for k, (a, b, c, e) in areas.items():
        print(f'  {k:10s} {d[b:e, a:c].mean():6.2f}')
out = np.concatenate([ref, our, np.clip(d * 3, 0, 255)], 0).astype(np.uint8)
Image.fromarray(out).save(sys.argv[3])
