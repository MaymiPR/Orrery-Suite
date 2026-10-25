// charsim: the character band's gain computer (Source/dsp/BandDynamics.h) as a C library for the fitting scripts
// (nulltest/charsim.py loads it with ctypes): band signal in, gain n(t) and wet reduction out.
#include "dsp/BandDynamics.h"

extern "C"
#ifdef _WIN32
__declspec (dllexport)
#endif
void band_gain (const double* mid, const double* side, int n, double fs, const double* params, int peak, int feedback,
                double* gainOut, double* grOut)
{
    ap::dsp::BandDynamics d;
    d.configure (params, peak, feedback != 0, fs);
    d.reset();
    for (int i = 0; i < n; ++i)
    {
        gainOut[i] = d.step (mid[i], side != nullptr ? side[i] : 0.0, false);
        if (grOut != nullptr) grOut[i] = d.gr;
    }
}
