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
        std::vector<float> buf ((size_t) 2 * N, 0.0f);
        std::copy (block.begin(), block.end(), buf.begin());
        fft.performRealOnlyForwardTransform (buf.data(), true);
        newest = (newest + 1) % parts;
        auto& spec = inputSpectra[(size_t) newest];
        for (int k = 0; k <= B; ++k) spec[(size_t) k] = { buf[(size_t) (2 * k)], buf[(size_t) (2 * k + 1)] };
        std::vector<std::complex<float>> acc ((size_t) B + 1);
        for (int p = 0; p < parts; ++p)
        {
            const auto& in = inputSpectra[(size_t) ((newest - p + parts) % parts)];
            const auto& h = spectra[(size_t) p];
            for (int k = 0; k <= B; ++k) acc[(size_t) k] += in[(size_t) k] * h[(size_t) k];
        }
        for (int k = 0; k <= B; ++k) { buf[(size_t) (2 * k)] = acc[(size_t) k].real(); buf[(size_t) (2 * k + 1)] = acc[(size_t) k].imag(); }
        fft.performRealOnlyInverseTransform (buf.data());
        // overlap-save: the second half is valid; it is the partitions' output for the next B samples
        for (int k = 0; k < B; ++k) tail[(size_t) k] = buf[(size_t) (B + k)];
    }

    juce::dsp::FFT fft { order };
    std::vector<float> head, hist, block, tail;
    std::vector<std::vector<std::complex<float>>> spectra, inputSpectra;
    int parts = 0, pos = 0, newest = 0, hpos = 0;
};
}
