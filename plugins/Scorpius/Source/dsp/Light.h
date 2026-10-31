#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

// The level the stripe's light follows (measured on the reference with tones, pulses, ramps and music, RESEARCH.md UI
// round 6), computed on the audio thread from the input after the input gain, in chunks of 10 ms:
//   - each chunk's sample peak (dBFS); the level takes it when it jumps: by more than 4.5 dB up or down against the
//     chunks of the last 120 ms, or after silence. Smooth changes leave it where it was (a tone ramp, however long,
//     keeps its starting brightness), while music, whose peaks come and go, is followed as it fades;
//   - when the sound stops (a chunk below -90 dBFS) the level rises to the loudest chunk of the last 0.15 s if that is
//     louder, and otherwise carries on from where it was (filmed with FL Studio's DL Clubbed loop, whose kicks decay
//     below -90 dBFS for ~40 ms before the next: the reference stays fully lit through them, and after the loop ends
//     it holds and fades as a level falling from where it was);
//   - it rises at once and falls at 50 dB/s.
// The editor reads it on its own clock (and the beats: the times it jumped up).
namespace sc
{
class Light
{
public:
    static constexpr float jumpDb = 4.5f, releaseDbPerSec = 50.0f, silenceDb = -90.0f;
    static constexpr int lookback = 12, stopChunks = 15;

    void prepare (double sampleRate)
    {
        chunk = juce::jmax (1, (int) std::lround (0.01 * sampleRate));
        chunkSeconds = (float) chunk / (float) sampleRate;
        reset();
    }
    void reset()
    {
        history.fill (-200.0f);
        pos = 0; count = 0; peak = 0.0f;
        target = detector = -200.0f;
        sounding = false;
        level = -200.0f;
    }
    void process (const juce::AudioBuffer<float>& in)
    {
        const int n = in.getNumSamples(), channels = juce::jmin (2, in.getNumChannels());
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < channels; ++c) peak = juce::jmax (peak, std::abs (in.getSample (c, i)));
            if (++count == chunk) endChunk();
        }
    }
    float levelDb() const { return level.load(); }
    int beatCount() const { return beats.load(); }

private:
    void endChunk()
    {
        const float pk = peak > 1.0e-10f ? 20.0f * std::log10 (peak) : -200.0f;
        peak = 0.0f; count = 0;
        const bool now = pk > silenceDb;
        if (now)
        {
            bool jump = ! sounding;
            for (int k = 1; k <= lookback && ! jump; ++k)
            {
                const float h = history[(size_t) ((pos - k + size) % size)];
                if (h > silenceDb && std::abs (pk - h) > jumpDb) jump = true;
            }
            if (jump)
            {
                if (pk > target) beats.fetch_add (1);
                target = pk;
            }
        }
        else if (sounding)
        {
            // (never lower than it was: a short gap in a decaying sample does not drop the light)
            for (int k = 1; k <= stopChunks; ++k) detector = juce::jmax (detector, history[(size_t) ((pos - k + size) % size)]);
            target = -200.0f;
        }
        history[(size_t) pos] = now ? pk : -200.0f;
        pos = (pos + 1) % size;
        sounding = now;
        detector = target >= detector ? target : juce::jmax (target, detector - releaseDbPerSec * chunkSeconds);
        level = detector;
    }

    static constexpr int size = 32;
    std::array<float, size> history {};
    int pos = 0, count = 0, chunk = 480;
    float peak = 0.0f, target = -200.0f, detector = -200.0f, chunkSeconds = 0.01f;
    bool sounding = false;
    std::atomic<float> level { -200.0f };
    std::atomic<int> beats { 0 };
};
}
