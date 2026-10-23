"""The character section predicted from the per-band laws (measurements/character/bandlaw.json), against the reference,
for any probe: checks that laws fitted at one tone frequency per band hold elsewhere (detector filtering, band overlap).

    python predict.py <freq Hz> <node>      static tone steps at that frequency (ident_signals levels)"""
import json, sys
import numpy as np
import tgp, ident as I, ident_signals as S, fit_bandlaw as F

NAMES = ['low', 'mid', 'high']


def predict(x, node, table):
    xb, sb = I.bands(x)[:, 0], I.nl_bands(x)[:, 0]
    M = [10 ** (I.GAINS[b][node] / 20) for b in range(3)]
    out = 0.0
    for b in range(3):
        law = table.get(f'{NAMES[b]}|{node}')
        out = out + M[b] * (F.band_out(xb[b], law, sb[b]) if law else xb[b])
    return out


if __name__ == '__main__':
    f, node = float(sys.argv[1]), int(sys.argv[2])
    table = json.load(open(F.OUT))
    x = S.tone_segments(f, [(S.STATIC_SECS, l) for l in S.STATIC_LEVELS])
    y = tgp.render_many([dict(x=x, norm={**tgp.CHAR_ONLY, 'Character': node / 8}, raw=True)], cache=True)[0][0]
    e = predict(x, node, table) - y
    for si, l in enumerate(S.STATIC_LEVELS):
        s = slice(S.PRE + int(si * S.STATIC_SECS * tgp.FS) + tgp.FS, S.PRE + int((si + 1) * S.STATIC_SECS * tgp.FS))
        g_ref = np.std(y[s]) / np.std(x[s])
        g_mod = np.std((e + y)[s]) / np.std(x[s])
        print(f'{l:4d} dBFS  residual {10 * np.log10(np.sum(e[s] ** 2) / np.sum(y[s] ** 2)):6.1f} dB   gain ref {20*np.log10(g_ref):7.3f}  model {20*np.log10(g_mod):7.3f} dB')
