#pragma once
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <complex>
#include <vector>

// A long FIR filter without latency: its first B taps directly, the rest by uniformly partitioned FFT convolution
// in blocks of B (those taps start B samples late, so a block's FFT part is ready in time). For the learned models'
// long linear filters (Learned.h).
namespace sc::dsp
{
class Convolver
{
public:
    static constexpr int order = 7, B = 1 << (order - 1), N = 2 * B;      // block 64, FFT 128

    void prepare (const std::vector<float>& taps)
    {
        head.assign (taps.begin(), taps.begin() + std::min<size_t> ((size_t) B, taps.size()));
        head.resize ((size_t) B, 0.0f);
        const int rest = (int) taps.size() - B;
        parts = std::max (0, (rest + B - 1) / B);
        spectra.assign ((size_t) parts, std::vector<std::complex<float>> ((size_t) B + 1));
        for (int p = 0; p < parts; ++p)
        {
            std::vector<float> buf ((size_t) 2 * N, 0.0f);
            for (int k = 0; k < B; ++k)
            {
                const size_t idx = (size_t) (B + p * B + k);
                if (idx < taps.size()) buf[(size_t) k] = taps[idx];
            }
            fft.performRealOnlyForwardTransform (buf.data(), true);
            for (int k = 0; k <= B; ++k) spectra[(size_t) p][(size_t) k] = { buf[(size_t) (2 * k)], buf[(size_t) (2 * k + 1)] };
        }
        inputSpectra.assign ((size_t) std::max (1, parts), std::vector<std::complex<float>> ((size_t) B + 1));
        work.assign ((size_t) 2 * N, 0.0f);
        acc.assign ((size_t) B + 1, {});
        reset();
    }
    void reset()
    {
        hist.assign ((size_t) B, 0.0f);
        block.assign ((size_t) N, 0.0f);
        tail.assign ((size_t) B, 0.0f);
        for (auto& s : inputSpectra) std::fill (s.begin(), s.end(), std::complex<float> {});
        pos = 0; newest = 0; hpos = 0;
    }
    // keeps the current state (prepare time), and goes back to it without allocating (the audio thread)
    void save() { saved = { hist, block, tail, inputSpectra, pos, newest, hpos }; }
    void restore()
    {
        std::copy (saved.hist.begin(), saved.hist.end(), hist.begin());
        std::copy (saved.block.begin(), saved.block.end(), block.begin());
        std::copy (saved.tail.begin(), saved.tail.end(), tail.begin());
        for (size_t p = 0; p < inputSpectra.size(); ++p) std::copy (saved.spectra[p].begin(), saved.spectra[p].end(), inputSpectra[p].begin());
        pos = saved.pos; newest = saved.newest; hpos = saved.hpos;
    }
    float process (float x)
    {
        // direct head
        hist[(size_t) hpos] = x;
        float y = 0;
        int j = hpos;
        for (int k = 0; k < B; ++k) { y += head[(size_t) k] * hist[(size_t) j]; j = j == 0 ? B - 1 : j - 1; }
        hpos = (hpos + 1) % B;
        // the partitions' share, computed at the last block boundary
        y += tail[(size_t) pos];
        block[(size_t) (B + pos)] = x;
        if (++pos == B)
        {
            pos = 0;
            runBlock();
            std::copy (block.begin() + B, block.end(), block.begin());     // (overlap-save: keep the last block)
        }
        return y;
    }

private:
    void runBlock()
    {
        if (parts == 0) { std::fill (tail.begin(), tail.end(), 0.0f); return; }
        auto& buf = work;                                                // (no allocation on the audio thread)
        std::fill (buf.begin(), buf.end(), 0.0f);
        std::copy (block.begin(), block.end(), buf.begin());
        fft.performRealOnlyForwardTransform (buf.data(), true);
        newest = (newest + 1) % parts;
        auto& spec = inputSpectra[(size_t) newest];
        for (int k = 0; k <= B; ++k) spec[(size_t) k] = { buf[(size_t) (2 * k)], buf[(size_t) (2 * k + 1)] };
        std::fill (acc.begin(), acc.end(), std::complex<float> {});
        for (int p = 0; p < parts; ++p)
        {
            const float* in = reinterpret_cast<const float*> (inputSpectra[(size_t) ((newest - p + parts) % parts)].data());
            const float* h = reinterpret_cast<const float*> (spectra[(size_t) p].data());
            float* a = reinterpret_cast<float*> (acc.data());
            for (int k = 0; k <= B; ++k)
            {
                const float ir = in[2 * k], ii = in[2 * k + 1], hr = h[2 * k], hi = h[2 * k + 1];
                a[2 * k] += ir * hr - ii * hi;
                a[2 * k + 1] += ir * hi + ii * hr;
            }
        }
        for (int k = 0; k <= B; ++k) { buf[(size_t) (2 * k)] = acc[(size_t) k].real(); buf[(size_t) (2 * k + 1)] = acc[(size_t) k].imag(); }
        fft.performRealOnlyInverseTransform (buf.data());
        // overlap-save: the second half is valid; it is the partitions' output for the next B samples
        for (int k = 0; k < B; ++k) tail[(size_t) k] = buf[(size_t) (B + k)];
    }

    juce::dsp::FFT fft { order };
    std::vector<float> head, hist, block, tail, work;
    std::vector<std::complex<float>> acc;
    std::vector<std::vector<std::complex<float>>> spectra, inputSpectra;
    int parts = 0, pos = 0, newest = 0, hpos = 0;
    struct State
    {
        std::vector<float> hist, block, tail;
        std::vector<std::vector<std::complex<float>>> spectra;
        int pos = 0, newest = 0, hpos = 0;
    } saved;
};
}
