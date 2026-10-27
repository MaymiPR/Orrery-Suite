#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>
#include <cmath>

// What the editor's animations follow, measured on the reference (RESEARCH.md, UI round 4) and computed on the audio
// thread from the input (after the input gain, both channels' mean):
//   - the circles follow a band's level read from a 4096-point spectrum (Hann, every 4096 samples: the reference's
//     circles move in steps of ~85 ms): the band's
//     magnitudes summed with weights, scaled so that a sine in the band reads as its amplitude. A sum of magnitudes is
//     what fits the reference: noise, spread over many bins, reads higher than a sine of the same RMS (pink noise at
//     -15 dBFS RMS reads 0.52 in the mids, a 1 kHz sine at -12 dBFS 0.25). The weights are the response measured with
//     sines: for harmonics the lows (flat to 80 Hz, -7 dB at 100 Hz, gone by 150 Hz; neither its amount nor its
//     50 / 80 Hz switch matters), for lift the mids (-24 dB at 300 Hz, at most at 1 ... 2 kHz, -31 dB at 6 ... 8 kHz).
// (The compression readout follows the compressor itself: dsp/Compressor.h.)
// The editor reads the band levels (the largest since it last asked) on its own clock.
namespace sc
{
class Visuals
{
public:
    static constexpr int order = 12, size = 1 << order, hop = size;

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
        lowScale = 1.0f / measureSine (40.0, lowWeight);
        midScale = 1.0f / measureSine (1000.0, midWeight);
        reset();
    }
    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        pos = 0; sinceHop = 0;
    }
    void process (const juce::AudioBuffer<float>& in)
    {
        const int n = in.getNumSamples();
        const float* l = in.getReadPointer (0);
        const float* r = in.getReadPointer (in.getNumChannels() > 1 ? 1 : 0);
        for (int i = 0; i < n; ++i)
        {
            const float x = 0.5f * (l[i] + r[i]);
            ring[(size_t) pos] = x;
            pos = (pos + 1) & (size - 1);
            if (++sinceHop == hop)
            {
                sinceHop = 0;
                analyse();
            }
        }
    }
    // the largest band level since the last call (linear: a sine reads its amplitude)
    float takeLows() { return lows.exchange (0.0f); }
    float takeMids() { return mids.exchange (0.0f); }

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
    float lowScale = 1.0f, midScale = 1.0f;
    int pos = 0, sinceHop = 0;
    std::atomic<float> lows { 0.0f }, mids { 0.0f };
};
}
