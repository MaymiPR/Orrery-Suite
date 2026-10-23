"""ctypes wrapper of build/charsim/Release/charsim.dll (tools/charsim.cpp): one character band's gain computer,
exactly the plug-in's (Source/dsp/BandDynamics.h)."""
import ctypes, os
import numpy as np
import tgp

_lib = ctypes.CDLL(os.path.join(tgp.ROOT, 'build', 'charsim', 'Release', 'charsim.dll'))
_D = ctypes.POINTER(ctypes.c_double)
_lib.band_gain.argtypes = [_D, _D, ctypes.c_int, ctypes.c_double, _D, ctypes.c_int, ctypes.c_int, _D, _D]
NAMES = ['makeup', 'mix', 'cubic', 'thresh', 'ratio', 'knee', 'attack', 'release', 'side', 'gainAttack', 'gainRelease',
         'attack2', 'release2', 'slowOffset', 'width', 'mixDb', 'shapeDry']


def band_gain(mid, params, side=None, fs=tgp.FS, peak=False, feedback=False, want_gr=False):
    mid = np.ascontiguousarray(mid, np.float64)
    side = None if side is None else np.ascontiguousarray(side, np.float64)
    p = np.zeros(len(NAMES))
    for k, v in params.items(): p[NAMES.index(k)] = v
    out = np.empty_like(mid); gr = np.empty_like(mid) if want_gr else None
    _lib.band_gain(mid.ctypes.data_as(_D), None if side is None else side.ctypes.data_as(_D), len(mid), fs,
                   p.ctypes.data_as(_D), int(peak), int(feedback), out.ctypes.data_as(_D), None if gr is None else gr.ctypes.data_as(_D))
    return (out, gr) if want_gr else out
