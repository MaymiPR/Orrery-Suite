"""Drive: the filter after the clip, measured directly. A tone at +24 dBFS turns the clip into a square-wave source
(levels +c+ / -c-, its timing set by the filter before the clip); the output's k-th harmonic is then
Y_k = Post(k f0) Q_k e^{j k phi(f0)}, Q_k = 2 (c+ + c-) / (pi k) for odd k. Working down from high frequencies, where
Post ~ 1, each tone's phase phi(f0) and Post at its odd harmonics follow; then Pre = L / Post. Writes
measurements/scorpius/drive_split_<colour>.json with Post and Pre on a frequency grid.
    python drive_split.py [colour]"""
import json, os, sys
import numpy as np
import orion as o

FS = 48000
OUT = os.path.join(o.ROOT, 'measurements', 'scorpius')


def harmonics_complex(y, x, f0, kmax=15):
    n = (len(y) // (FS // np.gcd(FS, int(f0)))) * (FS // np.gcd(FS, int(f0)))
    n = min(n, len(y)); t = np.arange(n) / FS
    ref = np.exp(-2j * np.pi * f0 * t)
    X1 = 2 * np.mean(x[:n] * ref)
    out = []
    for k in range(0, kmax + 1):
        Yk = (2 if k else 1) * np.mean(y[:n] * np.exp(-2j * np.pi * k * f0 * t))
        out.append(Yk / (X1 / abs(X1)) ** k if k else Yk)     # phase relative to the input's k-th power
    return np.array(out)


if __name__ == '__main__':
    colour = sys.argv[1] if len(sys.argv) > 1 else 'a'
    f0s = [int(round(v)) for v in np.geomspace(20, 3000, 70)]
    f0s = sorted(set(f0s))
    xs = [o.tone(f, 1.5, 24) for f in f0s]
    ys = o.render_many([dict(x=x, params=o.ONLY('drive', hipass=0), hidden={'drive.driveMode': colour}) for x in xs], cache=True)
    H = {f: harmonics_complex(y[0, 24000:].astype(float), x[24000:].astype(float), f) for f, x, y in zip(f0s, xs, ys)}
    # the square's levels from the highest tones (Post ~ 1 there): DC = (c+ - c-) / 2, fundamental 4 (c+ + c-) / (2 pi)
    hi = [f for f in f0s if f >= 1500]
    dc = np.mean([H[f][0].real for f in hi]); amp = np.mean([abs(H[f][1]) for f in hi])
    cs = amp * np.pi / 2                # c+ + c- over 2 ... the half swing
    print(colour, 'square: DC %.4f, half swing %.4f -> c+ %.4f c- %.4f' % (dc, cs, cs + dc, cs - dc))
    # Post on a grid, starting at 1 above 1.5 kHz, then each tone (high to low): phi from the highest odd harmonic whose
    # frequency already has a Post value, Post at its other odd harmonics
    post = {}
    for f in sorted(f0s, reverse=True):
        Y = H[f]
        known = [k for k in (1, 3, 5, 7, 9, 11, 13, 15) if k * f >= 1500 or any(abs(k * f - g) / g < 0.03 for g in post)]
        if not known:
            continue
        k = max(known)
        Pk = 1.0 if k * f >= 1500 else post[min(post, key=lambda g: abs(g - k * f))]
        Qk = 2 * 2 * cs / (np.pi * k)
        # Y_k = Pk Qk e^{j k phi}: phi modulo 2 pi / k; pick the one nearest the previous tone's phi
        base = np.angle(Y[k] / (Pk * Qk)) / k
        cands = base + 2 * np.pi * np.arange(k) / k
        prev = post.get('phi_prev', 0.0) if isinstance(post.get('phi_prev', 0.0), float) else 0.0
        phi = cands[np.argmin(np.abs(np.angle(np.exp(1j * (cands - prev)))))]
        post['phi_prev'] = float(phi)
        for kk in (1, 3, 5):
            fk = kk * f
            if fk < 1500:
                post.setdefault(fk, Y[kk] / (2 * 2 * cs / (np.pi * kk)) / np.exp(1j * kk * phi))
        post.setdefault(('phi', f), phi)
    grid = sorted(g for g in post if isinstance(g, (int, float)) and not isinstance(g, bool) and g != 'phi_prev')
    P = [(g, post[g]) for g in grid if not isinstance(post[g], float)]
    phis = sorted((k[1], v) for k, v in post.items() if isinstance(k, tuple))
    for g, v in P[::4]:
        print(f'  Post {g:6.0f} Hz: {20*np.log10(abs(v)):+6.2f} dB {np.degrees(np.angle(v)):+7.1f} deg')
    for g, v in phis[::6]:
        print(f'  pre phase at {g:5d} Hz: {np.degrees(v):+7.1f} deg')
    json.dump(dict(c_plus=cs + dc, c_minus=cs - dc, post=[(g, v.real, v.imag) for g, v in P], phi=phis),
              open(os.path.join(OUT, f'drive_split_{colour}.json'), 'w'))
