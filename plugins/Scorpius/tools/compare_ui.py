"""Side-by-side comparison of a Scorpius snapshot with the reference's screenshot (same scale): per region the
reference, ours and the difference (x4), stacked; and the mean absolute difference per region.
  python compare_ui.py reference/orion/ui200_idle.png ours.png out.png [scale=2]"""
import sys
import numpy as np
from PIL import Image, ImageDraw

ref = Image.open(sys.argv[1]).convert('RGB')
ours = Image.open(sys.argv[2]).convert('RGB')
out = sys.argv[3]
S = float(sys.argv[4]) if len(sys.argv) > 4 else 2.0
# regions in design px: name, x0, y0, x1, y1
REGIONS = [('top bar', 0, 0, 444, 31), ('top bar right', 444, 0, 888, 31), ('input + drive', 0, 31, 176, 272),
           ('harm comp lift', 175, 31, 425, 272), ('harmonic eq', 424, 31, 609, 272), ('parallels', 608, 31, 802, 272),
           ('output', 800, 31, 888, 272), ('stripe', 0, 268, 888, 312), ('plate left', 0, 308, 444, 404),
           ('plate right', 444, 308, 888, 404)]
w = int(888 * S)
tiles = []
for name, x0, y0, x1, y1 in REGIONS:
    box = tuple(int(v * S) for v in (x0, y0, x1, y1))
    a, b = ref.crop(box), ours.crop(box)
    da = np.asarray(a).astype(int); db = np.asarray(b).astype(int)
    diff = np.clip(np.abs(da - db) * 4, 0, 255).astype(np.uint8)
    print(f'{name:16s} mean abs diff {np.abs(da - db).mean():6.2f}')
    row = Image.new('RGB', (a.width * 3 + 20, a.height + 14), (40, 40, 40))
    row.paste(a, (0, 14)); row.paste(b, (a.width + 10, 14)); row.paste(Image.fromarray(diff), (2 * a.width + 20, 14))
    ImageDraw.Draw(row).text((2, 1), f'{name}: reference | ours | diff x4', fill=(255, 255, 0))
    tiles.append(row)
for i, t in enumerate(tiles):
    t.save(out.replace('.png', f'_{i}.png'))
