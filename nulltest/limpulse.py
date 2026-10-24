"""The limiter's inter-sample level estimate read off isolated pulses: a band-limited pulse of peak 2 (above the
ceiling) at a fractional position tau; the limiter's plan arrives at the pulse's peak level, so at the sample whose
interval holds the true peak, output / input = trim / level. Many positions tau show the estimate's phase grid and
its filter. Reference against limpy.py's designs.

    python limpulse.py"""
import numpy as np
import tgp, limpy

FS = tgp.FS; PRE = FS; GAP = 1500; TRIM = 10 ** (-0.1 / 20)
TAUS = np.arange(128) / 128
SHAPES = {'wide': 0.45, 'mid': 0.3, 'narrow': 0.15}     # cutoff re fs


def pulse(tau, fc, peak=2.0, half=40):
    n = np.arange(-half, half + 1) - tau
    h = 2 * fc * np.sinc(2 * fc * n) * np.kaiser(2 * half + 1, 8)
    tt = np.linspace(-0.5, 0.5, 4001)      # its true peak (at t = 0 of the continuous pulse)
    return h * peak / (2 * fc)


def signal(fc):
    n = PRE + GAP * len(TAUS); x = np.zeros(n); at = []
    for i, tau in enumerate(TAUS):
        p = pulse(tau, fc); a = PRE + i * GAP
        x[a:a + len(p)] += p; at.append(a + 40)        # the sample at or just before the true peak
    return x, at


def limiter_input(x):
    """what reaches the limiter, aligned with its output: our core path (exact) 38 samples early"""
    d = tgp.render_many([dict(x=np.stack([x, x]).astype(np.float32), norm=tgp.NONE)], 'model')[0][0]
    return np.roll(d, 38)


def read(xin, y, at):
    """the envelope's maximum around each pulse (the plan arrives at the pulse's level)"""
    out = []
    for a in at:
        s = slice(a, a + 400); big = np.abs(xin[s]) > 0.5
        out.append(np.max(np.abs(xin[s][big]) * TRIM / np.abs(y[s][big])))
    return np.array(out)


def ref_levels(fc):
    x, at = signal(fc); xin = limiter_input(x)
    y = tgp.render_many([dict(x=np.stack([x, x]).astype(np.float32), norm=tgp.LIM_ONLY, params={'Limiter Input Gain': 0})], 'ref', cache=True)[0][0]
    return x, xin, at, read(xin, y, at)


def port_levels(xin, at):
    lv, _ = limpy.levels(np.stack([xin, xin]))
    return np.array([np.max(lv[a:a + 400]) for a in at])


if __name__ == '__main__':
    for name, fc in SHAPES.items():
        x, xin, at, r = ref_levels(fc)
        o = port_levels(xin, at)
        y = tgp.render_many([dict(x=np.stack([x, x]).astype(np.float32), norm=tgp.LIM_ONLY, params={'Limiter Input Gain': 0})], 'model')[0][0]
        m = read(xin, y, at)
        print(f'{name}: ref level range {r.min():.5f} .. {r.max():.5f}; ours (plug-in) {m.min():.5f} .. {m.max():.5f}; '
              f'port - plug-in max {np.max(np.abs(o - m)):.2e}; ours - ref rms {np.sqrt(np.mean((m - r) ** 2)):.2e}')
        print('   tau: ' + ' '.join(f'{t:.3f}:{a:.5f}/{b:.5f}' for t, a, b in list(zip(TAUS, r, m))[::8]))
