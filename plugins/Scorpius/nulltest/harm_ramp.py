"""The harmonics amount during a jump (autom_probe.py's 'harm amount': 0.2 -> 0.9 at 1 s): which held amount each
stretch of the output looks like. The output in 32-sample windows after the change is fitted by the outputs held
still at amounts 0.2 ... 0.9 (the best one, then the best blend of it and a neighbour), for the reference and for
ours; with the null of ours against the reference at each held amount.
    python harm_ramp.py [block]"""
import sys
import numpy as np
import orion as o
from autom_probe import PRE, T1, T2, cases
from switch_on import switch_at
from order import drums

AMTS = [round(0.2 + 0.1 * k, 1) for k in range(8)]
W = 32


def best(y, held, s):
    """the held amount (fractional, between two neighbours) that fits y[:, s:s + W] best, and the fit's residual (dB)"""
    seg = y[:, s:s + W].ravel()
    out = (None, 1e9)
    for k in range(len(held) - 1):
        a, b = held[k][:, s:s + W].ravel(), held[k + 1][:, s:s + W].ravel()
        d = b - a
        w = np.clip(np.dot(seg - a, d) / (np.dot(d, d) + 1e-30), 0, 1)
        r = 10 * np.log10(np.sum((seg - a - w * d) ** 2) / (np.sum(seg ** 2) + 1e-30) + 1e-30)
        if r < out[1]: out = (AMTS[k] + 0.1 * w, r)
    return out


if __name__ == '__main__':
    block = int(sys.argv[1]) if len(sys.argv) > 1 else 512
    x = np.stack([drums()[:PRE + 3 * o.FS]] * 2) * 10 ** (-12 / 20)
    params, p, v0, v1 = cases()['harm amount']
    job = dict(x=x, params=params, auto=[(p, PRE + T1, v1), (p, PRE + T2, v0)])
    held = {}
    for who in ('ref', 'model'):
        held[who] = o.render_many([dict(x=x, params=dict(params, **{p: a})) for a in AMTS], who, block=block, cache=who == 'ref')
    print('held amount     ', ' '.join(f'{a:6.1f}' for a in AMTS))
    print('ours vs ref (dB)', ' '.join(f'{o.null_db(r, m):6.1f}' for r, m in zip(held['ref'], held['model'])))
    s1 = switch_at(PRE + T1, block)
    ys = {who: o.render_many([job], who, block=block, cache=who == 'ref')[0] for who in ('ref', 'model')}
    print(f'after the change (switch at {s1}): samples, the amount it looks like (fit residual dB), ours vs ref (dB)')
    for t in range(-64, 1600, W):
        s = s1 + t
        fr = best(ys['ref'], held['ref'], s); fm = best(ys['model'], held['model'], s)
        seg = ys['ref'][:, s:s + W]
        n = 10 * np.log10(np.sum((seg - ys['model'][:, s:s + W]) ** 2) / (np.sum(seg ** 2) + 1e-30) + 1e-30)
        print(f'{t:6d}  ref {fr[0]:5.2f} ({fr[1]:6.1f})   ours {fm[0]:5.2f} ({fm[1]:6.1f})   {n:6.1f}')
