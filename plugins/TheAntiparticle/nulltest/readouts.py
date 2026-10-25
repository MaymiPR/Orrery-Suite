"""Reads the I/O readouts (peak and RMS numbers above the LED meters) of every frame of a RefCapture film at 100 %.
The text is drawn the same, pixel for pixel, whenever the same string is in the same box, so the reader is a library of
crops read by eye: a frame's crop is looked up exactly. Crops not in the library yet are written to a contact sheet
(numbered), to be read and added to the labels file; nothing is guessed.
  python readouts.py film.raw labels.txt [other.raw=its_labels.txt ...]   (other films lend their read crops)
labels.txt lines:  "<time> <in_peak> <in_rms> <out_peak> <out_rms>"  (left channel at that frame; the right channel is
                   labelled the same, so the probe must be mono), "@<time> <io> <channel> <peak|rms> <value>" for one
                   crop, or "#<n> <value>" for crop n of the last unknowns sheet (python readouts.py --pin labels.txt
                   film.raw turns those into @ lines, which stay valid when the sheet changes)
Writes <film>.readouts.json (per frame: time and the eight values (in, out) x (L, R) x (peak, rms); -200 for -inf; None
where unknown) and, if there are unknowns, <film>.unknown.png and <film>.unknown.json."""
import json, os, sys
import numpy as np
from filmtools import Film

# the readout boxes: 40 px wide centred on each LED column, 16 px tall from y 44.5 (peak) and 67.5 (RMS)
CENTRES = {'in': (28.75, 65.25), 'out': (796 + 28.75, 796 + 65.25)}
TOPS = {'peak': 44, 'rms': 67}
KEYS = [(io, c, k) for io in ('in', 'out') for c in (0, 1) for k in ('peak', 'rms')]


def crop(fr, io, c, kind):
    cx = CENTRES[io][c]
    y = TOPS[kind]
    # brightness: the text is mint (or red when over 0 dBFS) on black
    return fr[y - 2:y + 18, int(cx - 21):int(cx + 21)].max(axis=2)


def library(film, path, lib):
    """adds the film's labelled crops to lib (crop bytes with the column parity -> value)"""
    unknown_file = film.path + '.unknown.json'
    unknown = json.load(open(unknown_file)) if os.path.exists(unknown_file) else []
    if True:
        for line in open(path):
            if not line.strip() or line.startswith('# '): continue
            if line.startswith('#') or line.startswith('@'):
                if line.startswith('#'):
                    n, v = line[1:].split()
                    t, key = unknown[int(n)]
                else:
                    t, io, c, kind, v = line[1:].split()
                    t, key = float(t), (io, int(c), kind)
                lib[(key[1], crop(film.at(t), *key).tobytes())] = -200.0 if v == '-inf' else float(v)
                continue
            t, *vals = line.split()
            fr = film.at(float(t))
            for (io, kind), v in zip([('in', 'peak'), ('in', 'rms'), ('out', 'peak'), ('out', 'rms')], vals):
                for c in (0, 1):
                    lib[(c, crop(fr, io, c, kind).tobytes())] = -200.0 if v == '-inf' else float(v)
    return lib


def read(film_path, label_path, lib=None, sheet=True):
    f = Film(film_path); f.path = film_path
    lib = library(f, label_path, {} if lib is None else lib)
    rows, unknown, seen = [], [], set()
    for i in range(f.n):
        fr = f.frame(i)
        crops = {key: (key[1], crop(fr, *key).tobytes()) for key in KEYS}
        # the probes are mono: a channel's crop takes the value the other channel's crop of the same readout shows
        for io, c, kind in KEYS:
            mine, other = crops[(io, c, kind)], crops[(io, 1 - c, kind)]
            if mine not in lib and other in lib: lib[mine] = lib[other]
        vals = []
        for key in KEYS:
            k = crops[key]
            v = lib.get(k)
            # (the right channel's crop is asked for only when the left one is known: once read, it carries over)
            if v is None and k not in seen and (key[1] == 0 or crops[(key[0], 0, key[2])] in lib):
                seen.add(k); unknown.append((float(f.t[i]), key))
            vals.append(v)
        rows.append([round(float(f.t[i]), 4)] + vals)
    json.dump(dict(keys=['t'] + ['_'.join(map(str, k)) for k in KEYS], rows=rows), open(film_path + '.readouts.json', 'w'))
    if unknown and sheet:
        from PIL import Image, ImageDraw
        json.dump(unknown, open(film_path + '.unknown.json', 'w'))
        tiles = []
        for n, (t, key) in enumerate(unknown):
            im = Image.fromarray(crop(f.at(t), *key)).resize((84, 40), Image.NEAREST)
            tile = Image.new('L', (130, 40), 0); tile.paste(im, (46, 0))
            ImageDraw.Draw(tile).text((2, 14), f'#{n}', fill=255)
            tiles.append(tile)
        ncol = 8; nrow = (len(tiles) + ncol - 1) // ncol
        out = Image.new('L', (130 * ncol, 40 * nrow), 40)
        for n, t in enumerate(tiles): out.paste(t, ((n % ncol) * 130, (n // ncol) * 40))
        out.save(film_path + '.unknown.png')
    return rows, unknown, lib


def pin(labels_path, film_path):
    unknown = json.load(open(film_path + '.unknown.json'))
    out = []
    for line in open(labels_path):
        if line.startswith('#') and not line.startswith('# '):
            n, v = line[1:].split()
            t, (io, c, kind) = unknown[int(n)]
            line = f'@{t:.4f} {io} {c} {kind} {v}\n'
        out.append(line)
    open(labels_path, 'w').writelines(out)


if __name__ == '__main__':
    if sys.argv[1] == '--pin':
        pin(sys.argv[2], sys.argv[3]); sys.exit()
    lib = {}
    for pair in sys.argv[3:]:
        other, its_labels = pair.split('=')
        lib = read(other, its_labels, lib, sheet=False)[2]
    rows, unknown, _ = read(sys.argv[1], sys.argv[2], lib)
    print('frames', len(rows), 'unknown crops', len(unknown))
