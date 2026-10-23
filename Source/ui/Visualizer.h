#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace dp
{
// The centre "particle" visualiser, measured on the reference (RESEARCH.md "Visualiser"):
//  - a black glass racetrack around the amount pill, drawn from the measured lighting field (GlassModel.h);
//  - while audio plays, a swarm of short-lived sparks swirls round the ring: a free-running 1 s loop at 30 frames/s
//    (the reference's sprite repeats every second and does not restart with the audio), ~290 sparks on screen,
//    drifting counter-clockwise at ~95 px/s with a large random scatter (median speed ~160 px/s), lifetimes mostly
//    under 0.2 s, the outer ones pulled inwards;
//  - on at once when audio starts, off 0.55 s after it stops (0.35 s fade); white at amount 0 % or with the
//    character section bypassed, blending linearly to full colour at 100 %.
// Drawn in editor design coordinates (the component covers the centre column).
class Visualizer : public juce::Component
{
public:
    Visualizer();
    void paint (juce::Graphics&) override;

    // called at 30 Hz by the editor: audio present this frame, colour mix 0 (white) .. 1 (colour), animation on/off
    void advance (bool audio, float colourMix, bool animate);

    // racetrack geometry (editor design px): the glass, and the amount pill on it (not quite concentric: measured)
    static constexpr float cx = 367.75f, cy = 148.25f, halfLength = 75.25f;
    static constexpr float pillRadius = 50.5f, outerRadius = 113.0f;           // sparks stay between these
    static constexpr float pillCx = 368.1f, pillCy = 148.9f, pillHalfLength = 74.7f, pillR = 49.75f;

private:
    struct Spark { float birth, life, x, y, vx, vy, size, bright; int hue; bool streak; };
    std::vector<Spark> sparks;
    float level = 0.0f;           // visibility 0 .. 1
    double silentFor = 10.0;      // seconds since audio was last present
    float colourMix = 1.0f;
    double clock = 0.0;           // free-running animation clock (s)
    juce::Image frame, lit, litGrey;  // the glass frame unlit / lit by the sparks (colour, white), per physical size
    float framePx = 0.0f;
    void renderFrame (float scale);
};
}
