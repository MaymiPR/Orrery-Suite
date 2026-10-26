"""Signal order (question 1): for two modules A and B, the reference with both on against A then B rendered one after
the other (each alone), and B then A. The order that nulls is the chain's; a parallel path is tested the same way
against a tap after each module. Pink noise with drum-like bursts, loud enough to drive every module.
    python order.py"""
import numpy as np
import orion as o


def drums(secs=4.0, seed=5):
    rng = np.random.default_rng(seed)
    n = int(secs * o.FS); t = np.arange(n) / o.FS
    x = 0.15 * o.noise(secs, 0, seed=seed, pink=True)
    for k in range(int(secs * 4)):
        i = int(k * 0.25 * o.FS); m = min(n - i, int(0.3 * o.FS)); tt = np.arange(m) / o.FS
        f = 50 + 120 * np.exp(-tt / 0.03)
        x[i:i + m] += 0.7 * np.sin(2 * np.pi * np.cumsum(f) / o.FS) * np.exp(-tt / 0.15)
        if k % 2: x[i:i + m] += 0.3 * rng.standard_normal(m) * np.exp(-tt / 0.05)
    return (x * 0.5).astype(np.float32)


SET = {'Harm Amount': 1, 'Comp Amount': 1, 'Lift Amount': 1, 'EQ 100Hz': 6, 'EQ 3kHz': 6,
       'Par Sub Level': 1, 'Par Low Level': 1, 'Par Loud Level': 1, 'Par Grit Level': 1}


def only(*m):
    return {**SET, **o.ONLY(*m, hipass=1)}


def chain(x, mods):
    y = x
    for m in mods: y = o.render(y, params=only(m), cache=True)
    return y


if __name__ == '__main__':
    x = drums()
    X = np.stack([x, x])
    serial = ['drive', 'harm', 'comp', 'lift', 'eq']
    print('modules in series (null of both-on against each order, dB):')
    for i in range(len(serial)):
        for j in range(i + 1, len(serial)):
            a, b = serial[i], serial[j]
            both = o.render(x, params=only(a, b), cache=True)
            ab, ba = chain(x, [a, b]), chain(x, [b, a])
            print(f'  {a:5s} {b:5s}: {a} then {b} {o.null_db(both, ab):6.1f}   {b} then {a} {o.null_db(both, ba):6.1f}')
    print('parallels: both-on against "module then parallel" and "parallel then module":')
    for p in ['sub', 'low', 'loud', 'grit']:
        for m in serial:
            both = o.render(x, params=only(m, p), cache=True)
            mp, pm = chain(x, [m, p]), chain(x, [p, m])
            print(f'  {p:4s} {m:5s}: {m} then {p} {o.null_db(both, mp):6.1f}   {p} then {m} {o.null_db(both, pm):6.1f}')
