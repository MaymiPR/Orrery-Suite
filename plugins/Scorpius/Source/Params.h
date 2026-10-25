#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// The host parameters (RESEARCH.md "Documented controls"): Orion's 26, with its names, ranges and display text, in its
// order; then the two settings Orion keeps out of the host's reach (drive colour, compression mode), which here are
// ordinary parameters so they can be automated. The defaults are the factory preset "start me up", which the
// reference opens with.
namespace sc
{
namespace id
{
inline constexpr const char* inputGain = "inputGain";
inline constexpr const char* driveOn = "driveOn";
inline constexpr const char* driveHiPass = "driveHiPass";
inline constexpr const char* harmOn = "harmOn";
inline constexpr const char* harmFreq = "harmFreq";
inline constexpr const char* harmAmount = "harmAmount";
inline constexpr const char* compOn = "compOn";
inline constexpr const char* compAmount = "compAmount";
inline constexpr const char* liftOn = "liftOn";
inline constexpr const char* liftAmount = "liftAmount";
inline constexpr const char* eqOn = "eqOn";
inline constexpr const char* eq40 = "eq40";
inline constexpr const char* eq100 = "eq100";
inline constexpr const char* eq1k = "eq1k";
inline constexpr const char* eq3k = "eq3k";
inline constexpr const char* eq10k = "eq10k";
inline constexpr const char* subOn = "subOn";
inline constexpr const char* subLevel = "subLevel";
inline constexpr const char* lowOn = "lowOn";
inline constexpr const char* lowLevel = "lowLevel";
inline constexpr const char* loudOn = "loudOn";
inline constexpr const char* loudLevel = "loudLevel";
inline constexpr const char* gritOn = "gritOn";
inline constexpr const char* gritLevel = "gritLevel";
inline constexpr const char* outputGain = "outputGain";
inline constexpr const char* driveColor = "driveColor";
inline constexpr const char* compMode = "compMode";
inline constexpr const char* bypass = "bypass";
}

// every parameter a preset (and the A / B compare) holds: all but the host bypass
inline const std::vector<const char*>& presetIds()
{
    static const std::vector<const char*> ids { id::inputGain, id::driveOn, id::driveHiPass, id::harmOn, id::harmFreq, id::harmAmount,
        id::compOn, id::compAmount, id::liftOn, id::liftAmount, id::eqOn, id::eq40, id::eq100, id::eq1k, id::eq3k, id::eq10k,
        id::subOn, id::subLevel, id::lowOn, id::lowLevel, id::loudOn, id::loudLevel, id::gritOn, id::gritLevel, id::outputGain,
        id::driveColor, id::compMode };
    return ids;
}

// the five harmonic EQ bands, left to right on the panel's bottom row and as the host lists them
inline constexpr const char* eqIds[5] { id::eq40, id::eq100, id::eq1k, id::eq3k, id::eq10k };
inline constexpr float eqFreqs[5] { 40.0f, 100.0f, 1000.0f, 3000.0f, 10000.0f };

// the parallels, left to right
struct Parallel { const char* on; const char* level; const char* name; };
inline constexpr Parallel parallels[4] { { id::subOn, id::subLevel, "sub" }, { id::lowOn, id::lowLevel, "low" },
                                         { id::loudOn, id::loudLevel, "loud" }, { id::gritOn, id::gritLevel, "grit" } };

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
