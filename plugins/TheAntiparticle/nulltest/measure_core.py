"""The always-present core path (every stage bypassed) and the limiter's look-ahead at several sample rates: delay
and the first-order all-pass (fitted: h = delay(D) * (a + z^-1) / (1 + a z^-1)). Writes measurements/core/core.json."""
import json, os
import numpy as np
from scipy.optimize import least_squares
from scipy.signal import lfilter
import tgp

OUT = os.path.join(tgp.ROOT, 'measurements', 'core'); os.makedirs(OUT, exist_ok=True)

if __name__ == '__main__':
    res = {}
    for fs in (44100, 48000, 88200, 96000, 192000):
        pre = 2 * fs; n = 4096
        x = np.zeros(pre + n, np.float32); x[pre] = 0.01
        ys = tgp.render_many([dict(x=x, norm=tgp.NONE, raw=True), dict(x=x, norm=tgp.LIM_ONLY, params={'Limiter Input Gain': 0}, raw=True),
                              dict(x=x, norm={}, raw=True)], fs=fs, cache=True)
        h = ys[0][0, pre:pre + n] / 0.01
        k = int(np.argmax(np.abs(h)))
        # fit D and a on the first samples (pure delay + first-order all-pass)
        best = None
        for D in range(max(0, k - 3), k + 2):
            def r(p):
                imp = np.zeros(n); imp[D] = 1
                return lfilter([p[0], 1], [1, p[0]], imp)[:k + 200] - h[:k + 200]
            s = least_squares(r, [0.5])
            if best is None or s.cost < best[2]: best = (D, s.x[0], s.cost)
        hl = ys[1][0, pre:pre + n] / 0.01 / 10 ** (-0.1 / 20)
        lag = int(np.argmax(np.correlate(hl[:2000], h[:2000], 'full'))) - 1999
        lat = tgp.latency(fs=fs)
        res[str(fs)] = dict(reported_latency=lat, core_delay=best[0], allpass_a=round(float(best[1]), 5), fit_rms=float(np.sqrt(best[2] * 2 / (k + 200))),
                            limiter_extra=lag, peak_all_on=int(np.argmax(np.abs(ys[2][0, pre:pre + n]))))
        print(fs, res[str(fs)])
    json.dump(res, open(os.path.join(OUT, 'core.json'), 'w'), indent=1)
