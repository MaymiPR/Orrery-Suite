"""Side-by-side zooms (reference | ours) of named regions of two 200 % screenshots:
   python tools/ui_zoom.py reference.png ours.png out.png [zoom]"""
import sys
from PIL import Image, ImageDraw

REGIONS = {  # 200 % pixel boxes
    'logo + preset': (0, 0, 470, 60), 'arrows + save': (740, 0, 940, 60), 'sidechain': (990, 0, 1300, 60),
    'ui scale + gear': (1480, 0, 1776, 60), 'input meter': (0, 60, 180, 280), 'eq': (180, 60, 354, 530),
    'gr top': (1118, 60, 1424, 290), 'gr bottom': (1118, 290, 1424, 530), 'limiter': (1422, 60, 1598, 280),
    'output': (1596, 60, 1776, 280), 'plate left': (0, 528, 360, 719), 'plate right': (1120, 528, 1776, 719),
    'letters': (340, 680, 1130, 719)}
ref, our = Image.open(sys.argv[1]).convert('RGB'), Image.open(sys.argv[2]).convert('RGB')
z = float(sys.argv[4]) if len(sys.argv) > 4 else 2.0
tiles = []
for name, box in REGIONS.items():
    w, h = box[2] - box[0], box[3] - box[1]
    t = Image.new('RGB', (int(2 * w * z) + 12, int(h * z) + 18), (60, 0, 0))
    t.paste(ref.crop(box).resize((int(w * z), int(h * z)), Image.NEAREST), (0, 18))
    t.paste(our.crop(box).resize((int(w * z), int(h * z)), Image.NEAREST), (int(w * z) + 12, 18))
    ImageDraw.Draw(t).text((4, 3), name + '   (reference | ours)', fill=(255, 255, 0))
    tiles.append(t)
W = max(t.width for t in tiles); H = sum(t.height for t in tiles)
out = Image.new('RGB', (W, H)); y = 0
for t in tiles: out.paste(t, (0, y)); y += t.height
out.save(sys.argv[3])
