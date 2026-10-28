"""The reference's oversampled path at other host rates (44.1 / 88.2 / 96 kHz): impulse renders of the bypassed path
(all stages off) at 1 / 2 / 4 / 8x and the reported latency, for os_kaiser.py. Switches the global setting through
the settings page (measure_oversampling.set_factor) and restores 1x."""
import json, os
import numpy as np
import tgp
from measure_oversampling import set_factor

RATES = (44100, 88200, 96000)
N = 32768


def jobs(fs, k):
    pre = 2 * fs
    imp = np.zeros(pre + N, np.float32); imp[pre] = 1e-3
    return [dict(x=imp, norm=tgp.NONE, raw=True, salt=f'os{k}')]


if __name__ == '__main__':
    lat = {}
    try:
        for k in (1, 2, 4, 8):
            set_factor(k)
            for fs in RATES:
                lat[f'{fs} {k}x'] = tgp.latency(fs=fs)
                tgp.render_many(jobs(fs, k), fs=fs, cache=True)
                print(fs, k, lat[f'{fs} {k}x'], flush=True)
    finally:
        set_factor(1)
    print('restored 1x; latency', tgp.latency())
    json.dump(lat, open(os.path.join(tgp.ROOT, 'measurements', 'oversampling', 'latency_rates.json'), 'w'), indent=1)
