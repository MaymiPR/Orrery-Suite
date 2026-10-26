#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>
#include <cmath>

// What the editor's animations follow, measured on the reference (RESEARCH.md, UI round 4) and computed on the audio
// thread from the input (after the input gain, both channels' mean):
//   - the circles follow a band's level read from a 4096-point spectrum (Hann, every 1024 samples): the band's
//     magnitudes summed with weights, scaled so that a sine in the band reads as its amplitude. A sum of magnitudes is
//     what fits the reference: noise, spread over many bins, reads higher than a sine of the same RMS (pink noise at
//     -15 dBFS RMS reads 0.52 in the mids, a 1 kHz sine at -12 dBFS 0.25). The weights are the response measured with
//     sines: for harmonics the lows (flat to 80 Hz, -7 dB at 100 Hz, gone by 150 Hz; neither its amount nor its
//     50 / 80 Hz switch matters), for lift the mids (-24 dB at 300 Hz, at most at 1 ... 2 kHz, -31 dB at 6 ... 8 kHz);
//   - the compression readout: until the compressor exists (Phase 4) a stand-in for its gain reduction, fitted to the
//     reference's readout with tones: 0.1 dB per dB of RMS (50 ms) above -28 dBFS (1 kHz at -10 dBFS: 1.5 dB, at -12:
//     1.3), scaled by the amount (the editor smooths and holds it as the reference's readout does). With the side-chain
//     on EXT it reads the side-chain instead (an empty buffer when the host leaves that bus off: no reduction).
//   - our own particle style (ui/Sky.h) follows the spectrum: 24 bands, log-spaced from 30 Hz to 16 kHz, each read
//     from the same spectrum as its RMS over its bins (a full-scale sine reads about 1).
// The editor reads the band levels (the largest since it last asked) on its own clock.
namespace sc
{
class Visuals
{
public:
    static constexpr int order = 12, size = 1 << order, hop = 1024, numBands = 24;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i < size; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size);
        static constexpr float liftHz[] { 150, 200, 300, 500, 700, 1000, 1500, 2000, 3000, 4000, 6000, 8000, 12000, 16000 };
        static constexpr float liftDb[] { -45, -40, -24, -15.4f, -5.6f, 0, -1.5f, -2.5f, -14.6f, -19, -31, -31, -45, -45 };
        static constexpr float lowHz[] { 20, 45, 60, 80, 100, 150, 200 };
        static constexpr float lowDb[] { 0, 0, -1, -1.3f, -7.4f, -45, -60 };
        for (int k = 0; k <= size / 2; ++k)
        {
            const float f = (float) (k * fs / size);
            lowWeight[(size_t) k] = response (f, lowHz, lowDb);
            midWeight[(size_t) k] = response (f, liftHz, liftDb);
        }
        // a sine where the response is 0 dB reads its amplitude
        for (int b = 0; b <= numBands; ++b)
        {
            const double hz = 30.0 * std::pow (16000.0 / 30.0, (double) b / numBands);
            bandEdge[(size_t) b] = juce::jlimit (1, size / 2, (int) std::lround (hz * size / fs));
        }
        for (int b = 0; b < numBands; ++b) bandEdge[(size_t) b + 1] = juce::jmax (bandEdge[(size_t) b + 1], bandEdge[(size_t) b] + 1);
        lowScale = 1.0f / measureSine (40.0, lowWeight);
        midScale = 1.0f / measureSine (1000.0, midWeight);
        rmsCoeff = (float) std::exp (-1.0 / (0.05 * sampleRate));
        reset();
    }
    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        pos = 0; sinceHop = 0; ms = 0.0f;
    }
    void process (const juce::AudioBuffer<float>& in, float compAmount, const juce::AudioBuffer<float>* key = nullptr)
    {
        const int n = in.getNumSamples();
        const float* l = in.getReadPointer (0);
        const float* r = in.getReadPointer (in.getNumChannels() > 1 ? 1 : 0);
        for (int i = 0; i < n; ++i)
        {
            const float x = 0.5f * (l[i] + r[i]);
            ring[(size_t) pos] = x;
            pos = (pos + 1) & (size - 1);
            if (key == nullptr) ms = rmsCoeff * ms + (1.0f - rmsCoeff) * x * x;
            if (++sinceHop == hop)
            {
                sinceHop = 0;
                analyse();
            }
        }
        if (key != nullptr)
        {
            const bool silent = key->getNumChannels() < 2 || key->getNumSamples() < n;
            for (int i = 0; i < n; ++i)
            {
                const float k = silent ? 0.0f : 0.5f * (key->getSample (0, i) + key->getSample (1, i));
                ms = rmsCoeff * ms + (1.0f - rmsCoeff) * k * k;
            }
        }
        const float rmsDb = 10.0f * std::log10 (ms + 1.0e-12f);
        grEstimate = juce::jmax (0.0f, 0.1f * (rmsDb + 28.0f)) * compAmount / 0.9f;
    }
    // the largest band level since the last call (linear: a sine reads its amplitude), and the gain-reduction stand-in
    float takeLows() { return lows.exchange (0.0f); }
    float takeMids() { return mids.exchange (0.0f); }
    void takeBands (float* out)
    {
        for (int b = 0; b < numBands; ++b) out[b] = bands[(size_t) b].exchange (0.0f);
    }
    std::atomic<float> grEstimate { 0.0f };

private:
    template <size_t n>
    static float response (float f, const float (&hz)[n], const float (&db)[n])
    {
        if (f <= hz[0]) return juce::Decibels::decibelsToGain (db[0]);
        for (size_t i = 1; i < n; ++i)
            if (f <= hz[i])
            {
                const float u = std::log (f / hz[i - 1]) / std::log (hz[i] / hz[i - 1]);
                return juce::Decibels::decibelsToGain (db[i - 1] + (db[i] - db[i - 1]) * u);
            }
        return juce::Decibels::decibelsToGain (db[n - 1]);
    }
    float weighted (const std::array<float, size / 2 + 1>& weights)
    {
        float sum = 0.0f;
        for (int k = 0; k <= size / 2; ++k) sum += weights[(size_t) k] * work[(size_t) k];
        return sum;
    }
    void transform (const float* samples)
    {
        std::fill (work.begin(), work.end(), 0.0f);
        for (int i = 0; i < size; ++i) work[(size_t) i] = samples[i] * window[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (work.data(), true);
    }
    float measureSine (double hz, const std::array<float, size / 2 + 1>& weights)
    {
        std::vector<float> sine ((size_t) size);
        for (int i = 0; i < size; ++i) sine[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / fs);
        transform (sine.data());
        return weighted (weights);
    }
    void analyse()
    {
        for (int i = 0; i < size; ++i) frame[(size_t) i] = ring[(size_t) ((pos + i) & (size - 1))];
        transform (frame.data());
        raiseTo (lows, lowScale * weighted (lowWeight));
        raiseTo (mids, midScale * weighted (midWeight));
        // (a Hann window's sine: peak bin size / 4 of the amplitude, its two neighbours half that)
        const float bandScale = 4.0f / ((float) size * 1.2247f);
        for (int b = 0; b < numBands; ++b)
        {
            float e = 0.0f;
            for (int k = bandEdge[(size_t) b]; k < bandEdge[(size_t) b + 1]; ++k) e += work[(size_t) k] * work[(size_t) k];
            raiseTo (bands[(size_t) b], bandScale * std::sqrt (e));
        }
    }
    static void raiseTo (std::atomic<float>& a, float v)
    {
        float cur = a.load();
        while (v > cur && ! a.compare_exchange_weak (cur, v)) {}
    }
    juce::dsp::FFT fft { order };
    double fs = 48000.0;
    std::array<float, size> ring {}, frame {}, window {};
    std::array<float, size * 2> work {};
    std::array<float, size / 2 + 1> lowWeight {}, midWeight {};
    float lowScale = 1.0f, midScale = 1.0f, rmsCoeff = 0.0f, ms = 0.0f;
    int pos = 0, sinceHop = 0;
    std::atomic<float> lows { 0.0f }, mids { 0.0f };
    std::array<int, numBands + 1> bandEdge {};
    std::array<std::atomic<float>, numBands> bands {};
    static_assert (numBands == 24, "the stripe's Sky reads 24 bands");
};
}
