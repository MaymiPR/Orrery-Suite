"""Draws the visualiser's glass (resources/glass/glass_off.png and glass_lit.png) from a vector description
(glass_params.json next to this script), so the textures are our own drawing: rim lines and faint sheen lines along
the track, the reflections as filled polygons with soft edges and smooth cubic shading, and, for the lit glass, the
light of the sparks as coloured vertical streaks from our own seeded noise, fading outwards and held back by the
reflections.

The layout is the plug-in's (Source/ui/Visualizer.cpp GlassField): x = station around the racetrack (25 per straight,
144 per curved end, 338 in all), y = radius 52 ... 114 design px in 0.5 px steps (125 rows), RGB. The shapes live in
those coordinates (station index, radius).

    python plugins/TheAntiparticle/tools/fit/draw_glass.py              writes both PNGs from glass_params.json
    python plugins/TheAntiparticle/tools/fit/draw_glass.py --trace off.png lit.png
        traces the reflections' outlines from a reference look into polygons and sets the shading, the lines'
        profiles and the light's falloff by least squares; writes glass_params.json (the references stay local)"""
import json, os, sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', '..', 'resources', 'glass')
PARAMS = os.path.join(HERE, 'glass_params.json')
NS, NR = 338, 125
ST = np.arange(NS) + 0.0
S, R = np.meshgrid(ST, 52.0 + 0.5 * np.arange(NR))
KNOTS = np.arange(0, NS + 1, 13.0)                      # profile control points along the track (wraps around)

# the reflections: name, station range, radius range, outline level (grey), drawn in this order
REGIONS = [('glow', (266, 337), (78, 109), 1.6), ('stroke', (112, 172), (79, 101), 8),
           ('lower', (192, 292), (88.6, 108), 16), ('upper', (220, 276), (79, 92.2), 16), ('cap', (224, 272), (71.6, 75.2), 12)]
# the faint lines along the track: radius, width
LINES = [(71.5, 0.45), (73.3, 0.35), (74.6, 0.35), (75.6, 0.35), (76.5, 0.4), (77.6, 0.35), (78.6, 0.35), (111.9, 0.5)]


def profile(vals):
    """a smooth periodic curve along the stations through control values at KNOTS (Catmull-Rom)"""
    v = np.asarray(vals, float); n = len(v) - 1; out = np.empty(NS)
    for i, s in enumerate(ST):
        x = s / 13.0; k = int(x); t = x - k
        p0, p1, p2, p3 = v[(k - 1) % n], v[k % n], v[(k + 1) % n], v[(k + 2) % n]
        out[i] = 0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (3 * (p1 - p2) + p3 - p0) * t ** 3)
    return out


def coverage(poly, soft):
    """0 ... 1 inside the polygon [(s, r), ...] with a soft edge of ~soft radius px (one station ~ half a radius px)"""
    p = np.asarray(poly, float)
    x, y = S / 2.0, R
    px, py = p[:, 0] / 2.0, p[:, 1]
    qx, qy = np.roll(px, -1), np.roll(py, -1)
    d = np.full(S.shape, np.inf); inside = np.zeros(S.shape, bool)
    for ax, ay, bx, by in zip(px, py, qx, qy):
        ex, ey = bx - ax, by - ay
        t = np.clip(((x - ax) * ex + (y - ay) * ey) / max(ex * ex + ey * ey, 1e-12), 0, 1)
        d = np.minimum(d, np.hypot(x - ax - t * ex, y - ay - t * ey))
        inside ^= ((ay > y) != (by > y)) & (x < ax + (y - ay) * ex / (by - ay + 1e-12))
    sd = np.where(inside, d, -d)
    return 0.5 * (1 + np.tanh(sd / max(soft, 0.05)))


def basis(s0, r0):
    """quartic shading terms in normalised coordinates around (s0, r0)"""
    u, v = (S - s0) / 20.0, (R - r0) / 8.0
    return np.stack([u ** i * v ** j for i in range(5) for j in range(5 - i)])


def ridge(r0, width):
    return np.exp(-0.5 * ((R - r0) / width) ** 2)


def draw(P, lit=False, seed=7):
    img = np.zeros((NR, NS, 3)) + np.asarray(P['base'])
    for (r0, w), prof, tint in zip(LINES, P['lines'], P['line_tints']):
        img += (ridge(r0, w) * np.maximum(profile(prof), 0))[..., None] * np.asarray(tint)
    hold = np.zeros(S.shape)
    for sh in P['shapes']:
        cov = coverage(sh['poly'], sh['soft'])
        shade = np.tensordot(np.asarray(sh['shade']), basis(*sh['centre']), 1)
        col = np.clip(shade, 0, 255)[..., None] * np.asarray(sh['tint'])
        img = img * (1 - cov[..., None]) + col * cov[..., None]
        hold = np.maximum(hold, cov * sh['hold'])
    if lit:
        L = P['light']
        rng = np.random.default_rng(seed)
        def smooth_noise(widths, count):                 # zero-mean, unit-sd periodic noise made of soft bumps
            v = np.zeros(NS)
            for _ in range(count):
                c = rng.uniform(0, NS); w = rng.uniform(*widths)
                v += rng.normal() * np.exp(-0.5 * (((ST - c + NS / 2) % NS - NS / 2) / w) ** 2)
            v -= v.mean(); return v / v.std()
        bright = 1 + L['contrast'] * smooth_noise(L['widths'], 160) + L['fine'] * smooth_noise(L['fine_widths'], 400)
        hue = np.stack([L['colour'] * smooth_noise(L['widths'], 160) for _ in range(3)], 1)
        hue -= hue.mean(1, keepdims=True)                # colour shifts that leave the brightness alone
        streak = np.asarray(L['tint'])[None, :] * (bright[:, None] + hue)
        fall = profile_r(L['fall'])
        img += (fall * (1 - hold))[..., None] * streak[None, :, :]
    return np.clip(img, 0, 255)


RK = np.arange(52.0, 114.01, 4.0)                       # radial falloff control points


def profile_r(vals):
    return np.interp(R, RK, vals)


def trace(ref_off, ref_lit):
    from matplotlib import pyplot as plt
    A = np.asarray(Image.open(ref_off).convert('RGB')).astype(float); g = A.mean(2)
    B = np.asarray(Image.open(ref_lit).convert('RGB')).astype(float)
    P = {'base': [float(np.median(A[..., c][g < 1.5])) for c in range(3)], 'shapes': []}
    fig = plt.figure()
    for name, (s0, s1), (ra, rb), level in REGIONS:
        box = (S >= s0) & (S <= s1) & (R >= ra) & (R <= rb)
        cs = plt.contour(ST, 52.0 + 0.5 * np.arange(NR), np.where(box, g, 0), levels=[level])
        polys = [q for path in cs.get_paths() for q in path.to_polygons() if len(q) > 8]
        area = lambda q: abs(np.sum(q[:, 0] * np.roll(q[:, 1], -1) - np.roll(q[:, 0], -1) * q[:, 1]))
        verts = max(polys, key=area)
        poly = rdp(verts, 0.18)
        inside = box & (coverage(poly, 0.05) > 0.5) & ((g < 12) if name == 'glow' else True)   # the glow under the highlight
        sc = (float(np.mean(S[inside])), float(np.mean(R[inside])))
        tint = A[inside].mean(0); tint = (tint / tint.max()).tolist()
        X = basis(*sc)[:, inside].T; yv = g[inside] / np.mean(tint)
        shade = np.linalg.lstsq(X, yv, rcond=None)[0]
        P['shapes'].append({'name': name, 'poly': np.round(poly, 3).tolist(), 'soft': 0.2, 'centre': list(sc),
                            'shade': shade.tolist(), 'tint': tint, 'hold': 1.0 if name != 'glow' else 0.0})
    plt.close(fig)
    from scipy.optimize import least_squares
    # each shape's edge softness, one at a time against the reference
    for sh in P['shapes']:
        def err(q):
            sh['soft'] = float(q[0]); return (draw_shapes_only(P) - A).ravel()
        sh['soft'] = float(least_squares(err, [0.2], bounds=([0.05], [0.8 if sh['name'] != 'glow' else 3.0]), diff_step=0.05).x[0])
    # the outlines' points, each within 1.5 px of the trace, and the shading again
    for sh in P['shapes']:
        name = sh['name']; (s0, s1), (ra, rb) = [(rg[1], rg[2]) for rg in REGIONS if rg[0] == name][0]
        box = (S >= s0 - 4) & (S <= s1 + 4) & (R >= ra - 2) & (R <= rb + 2)
        p0 = np.asarray(sh['poly'], float).ravel()
        def err(q):
            sh['poly'] = q.reshape(-1, 2).tolist(); return (draw_shapes_only(P) - A)[box].ravel()
        q = least_squares(err, p0, bounds=(p0 - 1.5, p0 + 1.5), diff_step=1e-3, max_nfev=25, x_scale=1.0).x
        sh['poly'] = np.round(q.reshape(-1, 2), 3).tolist()
        inside = (coverage(sh['poly'], 0.05) > 0.5) & ((g < 12) if name == 'glow' else True)
        X = basis(*sh['centre'])[:, inside].T
        sh['shade'] = np.linalg.lstsq(X, g[inside] / np.mean(sh['tint']), rcond=None)[0].tolist()
    # the lines' profiles: linear least squares, where no shape covers
    P['lines'] = [[0.0] * len(KNOTS) for _ in LINES]; P['line_tints'] = [[1.0, 1.0, 1.0] for _ in LINES]
    shapes = draw_shapes_only(P, base=True)
    free = (shapes.mean(2) - np.mean(P['base']) < 1.0)
    res = (A - np.asarray(P['base'])).mean(2)
    cols, idx = [], []
    for li, (r0, w) in enumerate(LINES):
        rg = ridge(r0, w)
        for k in range(len(KNOTS) - 1):
            e = np.zeros(len(KNOTS)); e[k] = 1; e[-1] = e[0]
            cols.append((rg * profile(e))[free]); idx.append((li, k))
    M = np.stack(cols, 1); lam = 2.0                  # a little ridge: no wild values where the shapes hide the lines
    coef = np.linalg.lstsq(np.vstack([M, np.sqrt(lam) * np.eye(M.shape[1])]), np.concatenate([res[free], np.zeros(M.shape[1])]), rcond=None)[0]
    for (li, k), c in zip(idx, coef): P['lines'][li][k] = float(np.clip(c, 0, 90))
    for li in range(len(LINES)): P['lines'][li][-1] = P['lines'][li][0]
    # the light of the sparks: its falloff from the lit reference, its streaks our own noise
    near = (B - A)[6:30].mean(0)                      # the light's statistics along the track (not its pattern)
    lum = near.mean(1)
    P['light'] = {'tint': (near.mean(0) / near.mean()).tolist(), 'contrast': float(lum.std() / lum.mean()),
                  'colour': float((np.abs(near - lum[:, None]).mean(1) / lum).mean()) * 1.25,
                  'widths': [4.0, 16.0], 'fine': 0.05, 'fine_widths': [0.6, 2.0], 'fall': [0.0] * len(RK)}
    off = draw(P); hold = np.zeros(S.shape)
    for sh in P['shapes']: hold = np.maximum(hold, coverage(sh['poly'], sh['soft']) * sh['hold'])
    extra = (B - off).mean(2) * (1 - hold)
    P['light']['fall'] = [float(np.median(extra[np.abs(R[:, 0] - rk) <= 2.0][:, :].ravel()) * 1.0) for rk in RK]
    json.dump(P, open(PARAMS, 'w'), indent=1)
    e0 = np.abs(draw(P) - A); e1 = np.abs(draw(P, lit=True).mean(2) - B.mean(2))
    print(f'off: mean |error| {e0.mean():.2f} grey, 99th percentile {np.percentile(e0, 99):.1f}; lit (grey): mean {e1.mean():.2f}')
    print('radial means lit ref / ours:', ' '.join(f'{a:.0f}/{b:.0f}' for a, b in zip(B.mean((1, 2))[::8], draw(P, lit=True).mean((1, 2))[::8])))


def draw_shapes_only(P, base=False):
    img = np.zeros((NR, NS, 3)) + np.asarray(P['base'])
    for sh in P['shapes']:
        cov = coverage(sh['poly'], sh['soft'])
        col = np.clip(np.tensordot(np.asarray(sh['shade']), basis(*sh['centre']), 1), 0, 255)[..., None] * np.asarray(sh['tint'])
        img = img * (1 - cov[..., None]) + col * cov[..., None]
    return img


def rdp(pts, eps):
    """Ramer-Douglas-Peucker simplification of a closed outline"""
    pts = np.asarray(pts, float)
    def rec(a, b):
        if b <= a + 1: return [a]
        p, q = pts[a], pts[b]; d = q - p; n = np.hypot(*d) or 1e-9
        w = pts[a + 1:b] - p; dist = np.abs(d[0] * w[:, 1] - d[1] * w[:, 0]) / n
        i = int(np.argmax(dist)) + a + 1
        return rec(a, i) + rec(i, b) if dist.max() > eps else [a]
    if np.allclose(pts[0], pts[-1]): pts = pts[:-1]
    far = int(np.argmax(np.hypot(*(pts - pts[0]).T)))          # a closed outline: two open halves
    pts = np.vstack([pts, pts[:1]])
    keep = rec(0, far) + rec(far, len(pts) - 1)
    return pts[keep]


def save_png(img, path):
    Image.fromarray(np.round(img).astype(np.uint8)).save(path)


if __name__ == '__main__':
    if '--trace' in sys.argv:
        i = sys.argv.index('--trace'); trace(sys.argv[i + 1], sys.argv[i + 2])
    P = json.load(open(PARAMS))
    os.makedirs(OUT, exist_ok=True)
    save_png(draw(P), os.path.join(OUT, 'glass_off.png'))
    save_png(draw(P, lit=True), os.path.join(OUT, 'glass_lit.png'))
    print('wrote', os.path.normpath(os.path.join(OUT, 'glass_off.png')), 'and glass_lit.png')
