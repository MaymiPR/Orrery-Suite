"""Residual of one stress case over time (0.1 s windows, dB re the reference's energy in the window), to see where a
stress null loses: after which parameter change, for how long.

    python timeres.py "bypass toggles" [--fs 48000] [--block 512] [--tune bypassLim=1 ...] [--quiet]"""
import argparse
import numpy as np
import tgp, stress as S

ap = argparse.ArgumentParser()
ap.add_argument('name'); ap.add_argument('--fs', type=int, default=48000); ap.add_argument('--block', type=int, default=512)
ap.add_argument('--win', type=float, default=0.1)
ap.add_argument('--tune', nargs='*', default=[])
ap.add_argument('--quiet', action='store_true')
a = ap.parse_args()
case = [c for _, cs in S.groups() for c in cs if a.name in c[0]][0]
name, s, norm, params, auto = case
x = S.SIG[s]()
x = np.concatenate([np.zeros((2, S.PRE) if x.ndim == 2 else S.PRE, np.float32), x], axis=-1)
j = dict(x=x, norm=norm, params=params)
if auto: j['auto'] = [(k, int(round(t * a.fs / S.FS)), v) for k, t, v in auto]
r = tgp.render_many([j], 'ref', fs=a.fs, block=a.block, cache=True)[0]
o = tgp.render_many([dict(j, tune={k: float(v) for k, v in (t.split('=') for t in a.tune)})], 'model', fs=a.fs, block=a.block)[0]
W = int(a.win * a.fs)
print(name, a.tune, 'total %.2f dB' % (10 * np.log10(np.sum((r[:, S.PRE:] - o[:, S.PRE:]) ** 2) / np.sum(r[:, S.PRE:] ** 2))))
if a.quiet: raise SystemExit
for i in range(S.PRE, r.shape[1] - W, W):
    e = np.sum((r[:, i:i + W] - o[:, i:i + W]) ** 2); ref = np.sum(r[:, i:i + W] ** 2)
    g = np.sqrt(np.sum(o[:, i:i + W] ** 2) / max(ref, 1e-30))
    print(f'{(i - S.PRE) / a.fs:6.2f} s  resid {10 * np.log10(e / max(ref, 1e-30) + 1e-30):7.1f} dB   level ours/ref {20 * np.log10(g + 1e-30):+7.3f} dB')
