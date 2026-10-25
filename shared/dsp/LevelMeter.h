#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

// The I/O meters of Cradle's plug-ins, as measured on The God Particle (plugins/TheAntiparticle/MEASUREMENTS.md 6) and
// seen again on Orion: a green LED bar on the RMS, a mint run above it up to a fast peak, and two readouts.
//   LevelTap:   what the audio thread hands the editor for one channel
//   LevelMeter: the audio-thread side for a stereo pair (call measure() on the signal at the meter's point)
//   Readouts:   the editor side of one channel (peak held and sampled, RMS live), on the reference's own clock
namespace orrery
{
struct LevelTap
{
    std::atomic<float> peak { 0.0f };        // highest sample since the editor last took it (linear): is audio playing?
    std::atomic<float> blockPeak { 0.0f };   // the latest block's sample peak (linear): the peak readout samples it
    std::atomic<float> meanSquare { 0.0f };  // latest mean square for the RMS readout (plain; one-pole on x^2, 89 ms)
    std::atomic<float> bar { 0.0f };         // latest RMS for the green bar (linear, plain; one-pole on x^2, 150 ms)
    std::atomic<float> fast { 0.0f };        // highest fast peak (|x| follower, 1 ms / 50 ms) since the editor took it
    void push (float blockPk, float blockMeanSquare, float blockBar, float blockFast)
    {
        auto raise = [] (std::atomic<float>& a, float v) { float p = a.load(); while (v > p && ! a.compare_exchange_weak (p, v)) {} };
        raise (peak, blockPk);
        raise (fast, blockFast);
        blockPeak.store (blockPk);
        meanSquare.store (blockMeanSquare);
        bar.store (blockBar);
    }
    float takePeak() { return peak.exchange (0.0f); }
    float takeFast() { return fast.exchange (0.0f); }
};

class LevelMeter
{
public:
    LevelTap left, right;

    void prepare (double sampleRate)
    {
        rmsCoef = (float) std::exp (-1.0 / (0.089 * sampleRate));
        barCoef = (float) std::exp (-1.0 / (0.15 * sampleRate));
        fastAttack = 1.0f - (float) std::exp (-1.0 / (0.001 * sampleRate));
        fastRelease = (float) std::exp (-1.0 / (0.05 * sampleRate));
        for (auto& s : state) s = {};
    }

    // one block of a stereo signal (a mono buffer meters both sides the same)
    void measure (const juce::AudioBuffer<float>& b)
    {
        LevelTap* taps[2] { &left, &right };
        for (int c = 0; c < 2; ++c)
        {
            const float* x = b.getReadPointer (juce::jmin (c, b.getNumChannels() - 1));
            auto& st = state[c];
            float pk = 0.0f, fastMax = 0.0f;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float a = std::abs (x[i]), sq = x[i] * x[i];
                pk = juce::jmax (pk, a);
                st.ms = sq + rmsCoef * (st.ms - sq);
                st.bar = sq + barCoef * (st.bar - sq);
                st.fast = a > st.fast ? st.fast + (a - st.fast) * fastAttack : st.fast * fastRelease;
                fastMax = juce::jmax (fastMax, st.fast);
            }
            taps[c]->push (pk, st.ms, std::sqrt (st.bar), fastMax);
        }
    }

private:
    struct State { float ms = 0.0f, bar = 0.0f, fast = 0.0f; } state[2];
    float rmsCoef = 0.0f, barCoef = 0.0f, fastAttack = 0.0f, fastRelease = 0.0f;
};

// dB with -200 for silence
inline float toDb (float lin) { return lin > 1.0e-6f ? juce::Decibels::gainToDecibels (lin) : -200.0f; }

// a readout's text: the reference shows -96.0 and, one step lower, -inf
inline juce::String dbText (float db) { return db < -96.0f ? juce::String ("-inf") : juce::String (db, 1); }

// The editor side of one meter channel. update() every repaint (LED bar, mint run, "is audio playing"); sample() on
// the reference's readout clock, every 46 ms: the peak readout takes the latest block's peak and holds the highest
// (until clicked), and every fifth time the RMS readout shows the plain RMS + 1.5 dB, falling 11.27 dB per update
// when no audio arrives; below -96 dB it reads -inf.
struct Readouts
{
    float peakHoldDb = -200.0f, rmsDb = -200.0f, rmsMeanSquare = 0.0f;
    float barDb = -200.0f, fastDb = -200.0f, rawPeakDb = -200.0f;

    void update (LevelTap& tap)
    {
        rawPeakDb = toDb (tap.takePeak());
        barDb = toDb (tap.bar.load());
        fastDb = toDb (tap.takeFast());
    }
    void samplePeak (LevelTap& tap) { peakHoldDb = juce::jmax (peakHoldDb, toDb (tap.blockPeak.load())); }
    void sampleRms (LevelTap& tap, bool running)
    {
        constexpr float silenceStep = 0.0747f;          // 10^(-11.27 / 10): 231 ms of the 89 ms one-pole
        rmsMeanSquare = running ? tap.meanSquare.load() : rmsMeanSquare * silenceStep;
        rmsDb = rmsMeanSquare > 1.0e-20f ? 10.0f * std::log10 (rmsMeanSquare) + 1.5f : -200.0f;
    }
};
}
