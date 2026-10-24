"""Limiter rule variants tried with the Python port (limpy.py) on the null tests' limiter signals: the residual of the
limiter's output against the reference's, with the limiter's input taken from our own render (limenv.py).

    python limrules.py"""
import sys
import numpy as np
import limenv, limpy, nulltest as N

CASES = [('pink -6', 5), ('drums -6', 12), ('music -2', 5)]


def residual(r, xin, e):
    y = xin * limenv.TRIM / e
    d = (r - y)[:, N.PRE:]
    return 10 * np.log10(np.sum(d ** 2) / np.sum(r[:, N.PRE:] ** 2))


if __name__ == '__main__':
    data = [limenv.envelopes(s, g) for s, g in CASES]
    variants = {'plug-in': {}, 'sample peaks': dict(rule=('sample',)), 'wide2': dict(rule=('wide2',)),
                'steep': dict(rule=('steep',)), 'prom 0.01': dict(prom=0.01), 'prom 0.05': dict(prom=0.05)}
    print('plug-in itself: ' + '  '.join(f'{residual(r, xin, np.max(np.abs(xin), 0) / np.maximum(np.max(np.abs(o), 0), 1e-30) * limenv.TRIM):7.2f}' for r, o, xin, er, eo in data))
    for name, kw in variants.items():
        print(f'{name:14s} ' + '  '.join(f'{residual(r, xin, limpy.env(xin, **kw)):7.2f}' for r, o, xin, er, eo in data), flush=True)
