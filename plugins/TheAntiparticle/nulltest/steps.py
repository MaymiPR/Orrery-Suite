"""Parameter step responses, the reference against ours: quiet pink noise (-30 dBFS RMS, nothing compresses), one
parameter stepped at an offset inside a host block, only the stage it belongs to enabled. The gain (output RMS over
16-sample windows re the output before the step) shows when each plug-in starts to react and how it ramps.

    python steps.py [name ...]"""
import sys
import numpy as np
import tgp

FS = tgp.FS
PRE = 2 * FS
B = 512
W = 16


def pink(secs=4.0, db=-30):
    return tgp.noise(secs, db, seed=31, pink=True)


TONE = {'eq low +6': 60.0, 'eq high -6': 12000.0, 'eq bypass on': 12000.0}     # lock-in frequency per step (else 1 kHz)


def tone_resp(name, offset=200, tune=None, db=-30, span=(-256, 1536)):
    """gain (dB) and phase (deg) of a steady tone around the step, per 1-sample hop over one-period windows"""
    norm, params, (k, v) = STEPS[name]
    f = TONE.get(name, 1000.0)
    n = PRE + 4 * FS; t = np.arange(n) / FS
    x = (10 ** (db / 20) * np.sin(2 * np.pi * f * t)).astype(np.float32); x[:PRE // 2] = 0
    at = PRE + FS + offset - (PRE + FS) % B
    j = dict(x=x, norm=norm, params=params, auto=[(k, at, v)])
    r = tgp.render_many([j], 'ref', cache=True)[0][0]
    o = tgp.render_many([dict(j, tune=tune or {})], 'model')[0][0]
    P = max(16, int(round(FS / f)))
    ref0 = None; out = []
    for y in (r, o):
        z = y * np.exp(-2j * np.pi * f * t)
        c = np.convolve(z, np.ones(P) / P, 'same')
        c0 = np.mean(c[at - 3000:at - 1000])
        out.append(c[at + span[0]:at + span[1]] / c0)
    blk = at - (at % B)
    return at - blk, np.arange(at + span[0], at + span[1]) - blk, out


STEPS = {   # name: (stage preset, start params, (param, normalised value after))
    'input gain +12': ({**tgp.NONE}, {'Input Gain': 0}, ('Input Gain', 1.0)),
    'output gain -12': ({**tgp.NONE}, {'Output Gain': 0}, ('Output Gain', 0.0)),
    'eq low +6': (tgp.EQ_ONLY, {}, ('EQ Low Gain', 1.0)),
    'eq high -6': (tgp.EQ_ONLY, {}, ('EQ High Gain', 0.0)),
    'eq bypass on': (tgp.EQ_ONLY, {'EQ High Gain': 6}, ('EQ Bypass', 1.0)),
    'amount 100 -> 200': (tgp.CHAR_ONLY, {}, ('Character', 1.0)),
    'amount 100 -> 0': (tgp.CHAR_ONLY, {}, ('Character', 0.0)),
    'limiter gain 5 -> 12': (tgp.LIM_ONLY, {}, ('Limiter Input Gain', 1.0)),
    'char bypass on': (tgp.CHAR_ONLY, {}, ('Character Bypass', 1.0)),
}


def response(name, offset=200, tune=None):
    norm, params, (k, v) = STEPS[name]
    x = pink(); x = np.concatenate([np.zeros(PRE, np.float32), x])
    at = PRE + FS + offset - (PRE + FS) % B             # a block start + offset
    j = dict(x=x, norm=norm, params=params, auto=[(k, at, v)])
    after = dict(norm); after[k] = v
    jobs = [j, dict(x=x, norm=after, params=params)] if k in ('EQ Bypass', 'Character Bypass') else [j]
    r = tgp.render_many(jobs, 'ref', cache=True)
    o = tgp.render_many([dict(jj, tune=tune or {}) for jj in jobs], 'model')
    def gains(y):
        e = np.array([np.sqrt(np.mean(y[0][a:a + W] ** 2)) for a in range(at - 256, at + 1024, W)])
        return 20 * np.log10(e / np.sqrt(np.mean(y[0][at - 4096:at - 256] ** 2)))
    return at, gains(r[0]), gains(o[0]), (r, o)


if __name__ == '__main__':
    if sys.argv[1:2] == ['--tone']:
        for nm in sys.argv[2:]:
            off, ts, (cr, co) = tone_resp(nm)
            print(f'== {nm} (tone): change {off} samples into the block; gain dB / phase deg')
            for i in range(0, len(ts), 32):
                print(f'   {ts[i]:+6d}  ref {20*np.log10(abs(cr[i])):+7.3f} {np.degrees(np.angle(cr[i])):+6.1f}   ours {20*np.log10(abs(co[i])):+7.3f} {np.degrees(np.angle(co[i])):+6.1f}')
        raise SystemExit
    names = sys.argv[1:] or list(STEPS)
    for nm in names:
        at, gr, go, _ = response(nm)
        blk = at - (at % B)
        print(f'== {nm}  (change set before the block starting {at - blk} samples before the nominal time)')
        rows = [(a - blk, g1, g2) for a, g1, g2 in zip(range(at - 256, at + 1024, W), gr, go)]
        for t, g1, g2 in rows[::2]:
            print(f'   {t:+6d}  ref {g1:+7.2f}  ours {g2:+7.2f}')
