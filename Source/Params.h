#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

// Host parameters, identical to the reference's list (names, order, ranges, tapers, defaults and display text;
// RESEARCH.md "Documented controls"). The IDs are ours; the names are what hosts show.
namespace ap
{
namespace id
{
inline constexpr const char* inputGain = "inputGain";              // -12 .. +12 dB, default 0
inline constexpr const char* outputGain = "outputGain";            // -12 .. +12 dB, default 0
inline constexpr const char* eqLow = "eqLow";                      // -6 .. +6 dB, default 0
inline constexpr const char* eqMid = "eqMid";                      // -6 .. +6 dB, default 0
inline constexpr const char* eqHigh = "eqHigh";                    // -6 .. +6 dB, default 0
inline constexpr const char* eqBypass = "eqBypass";                // 0 / 1
inline constexpr const char* character = "character";              // 0 .. 1 (the UI shows 0 .. 200 %), default 0.5
inline constexpr const char* characterBypass = "characterBypass";  // 0 / 1
inline constexpr const char* limiterGain = "limiterGain";          // 0 .. +12 dB, measured taper, default +5
inline constexpr const char* limiterBypass = "limiterBypass";      // 0 / 1
inline constexpr const char* bypass = "bypass";                    // host bypass, Off / On
}

// Limiter Input Gain taper: dB at normalised 0, 0.025, ..., 1 (the reference's display, RESEARCH.md); linear between.
inline constexpr std::array<float, 41> limiterTaper {
    0.000f, 0.221f, 0.445f, 0.671f, 0.901f, 1.135f, 1.371f, 1.611f, 1.854f, 2.101f, 2.351f, 2.605f, 2.863f, 3.124f,
    3.389f, 3.659f, 3.932f, 4.209f, 4.491f, 4.777f, 5.068f, 5.363f, 5.662f, 5.967f, 6.276f, 6.591f, 6.910f, 7.235f,
    7.565f, 7.901f, 8.242f, 8.589f, 8.942f, 9.301f, 9.667f, 10.038f, 10.417f, 10.802f, 11.194f, 11.593f, 12.000f };

inline float limiterDbFromNorm (float v)
{
    const float x = juce::jlimit (0.0f, 1.0f, v) * 40.0f;
    const int i = juce::jmin (39, (int) x);
    return limiterTaper[(size_t) i] + (limiterTaper[(size_t) i + 1] - limiterTaper[(size_t) i]) * (x - (float) i);
}

inline float limiterNormFromDb (float db)
{
    db = juce::jlimit (0.0f, 12.0f, db);
    int i = 0;
    while (i < 39 && limiterTaper[(size_t) i + 1] < db) ++i;
    const float a = limiterTaper[(size_t) i], b = limiterTaper[(size_t) i + 1];
    return ((float) i + (b > a ? (db - a) / (b - a) : 0.0f)) / 40.0f;
}

inline constexpr float limiterDefaultDb = 5.0f;   // normalised 0.494206 in the reference

// "-12.000", "0.500", "1.000": the reference prints every value with three decimals and no unit
inline juce::String threeDecimals (float v, int) { return juce::String (v, 3); }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
