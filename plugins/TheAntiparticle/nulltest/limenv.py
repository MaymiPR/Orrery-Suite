"""The limiter's envelope (1 / gain, in units of its ceiling) on program material, the reference's and ours, sample
by sample: the limiter's input is our own render of the same path with the limiter switched off (the core path is
exact; that render comes out 38 samples early), times the limiter's input gain.

    python limenv.py "pink -6" 5      (signal from nulltest.SIGNALS, limiter input gain dB) -> worst events"""
import sys
import numpy as np
import tgp, nulltest as N

TRIM = 10 ** (-0.1 / 20)


def envelopes(sig, gain_db, fs=48000, tune=None):
    x = np.atleast_2d(N.SIGNALS[sig]()); x = np.vstack([x, x]) if x.shape[0] == 1 else x
    x = np.concatenate([np.zeros((2, N.PRE), np.float32), x], -1)
    j = dict(x=x, norm=tgp.LIM_ONLY, params={'Limiter Input Gain': gain_db})
    r = tgp.render_many([j], 'ref', fs=fs, cache=True)[0]
    o = tgp.render_many([dict(j, tune=tune or {})], 'model', fs=fs)[0]
    d = tgp.render_many([dict(x=x, norm=tgp.NONE)], 'model', fs=fs)[0]
    la = {44100: 40, 48000: 38, 88200: 50, 96000: 54, 192000: 96}[fs]
    xin = np.roll(d, la, axis=1) * 10 ** (gain_db / 20)
    big = np.max(np.abs(xin), 0)
    with np.errstate(all='ignore'):
        er = np.where(big > 0.3, np.max(np.abs(xin), 0) / np.max(np.abs(r), 0) * TRIM, np.nan)
        eo = np.where(big > 0.3, np.max(np.abs(xin), 0) / np.max(np.abs(o), 0) * TRIM, np.nan)
    return r, o, xin, er, eo


if __name__ == '__main__':
    sig, g = sys.argv[1], float(sys.argv[2])
    r, o, xin, er, eo = envelopes(sig, g)
    e = r - o
    W = 240
    en = np.array([np.sum(e[:, i:i + W] ** 2) for i in range(N.PRE, r.shape[1] - W, W)])
    print('residual', 10 * np.log10(np.sum(e[:, N.PRE:] ** 2) / np.sum(r[:, N.PRE:] ** 2)))
    for j in np.argsort(en)[::-1][:3]:
        a = N.PRE + j * W; c = a + np.argmax(np.sum(e[:, a:a + W] ** 2, 0))
        print(f'event at {c}:')
        for k in range(c - 30, c + 20):
            print(f'   {k - c:+4d} in {np.max(np.abs(xin[:, k])):6.3f}  env ref {er[k]:7.3f} ours {eo[k]:7.3f}')
