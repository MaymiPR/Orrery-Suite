#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace dp
{
// The centre "particle" visualiser: a black glass racetrack around the amount pill, with particles that appear while
// audio plays (on at once, off 0.55 s after the audio stops, 0.35 s fade; measured on the reference). Particles are
// white at amount 0 % or with the character section bypassed, and blend linearly to full colour at 100 %.
// Drawn in editor design coordinates (the component covers the centre column).
class Visualizer : public juce::Component
{
public:
    Visualizer();
    void paint (juce::Graphics&) override;

    // called at 30 Hz by the editor: audio present this frame, colour mix 0 (white) .. 1 (colour), animation on/off
    void advance (bool audio, float colourMix, bool animate);

    // racetrack geometry (editor design px)
    static constexpr float cx = 367.75f, cy = 148.25f, halfLength = 75.25f;
    static constexpr float pillRadius = 50.5f, outerRadius = 113.0f;

private:
    struct Particle { float u, d, speed, size, phase, twinkle; int hue; bool streak; };
    std::vector<Particle> particles;
    float level = 0.0f;           // visibility 0 .. 1
    double silentFor = 10.0;      // seconds since audio was last present
    float colourMix = 1.0f;
    float time = 0.0f;
    juce::Image frame;            // the static glass frame, cached per physical size
    float framePx = 0.0f;
    juce::Point<float> racetrack (float u, float radius) const;
    void renderFrame (float scale);
};
}
