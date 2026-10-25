#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

// The visualiser stripe's light, over the dark tube the editor paints: a glow and a field of small coloured particles,
// as the reference shows while audio plays (filmed at 200 % with RefCapture, plugins/Scorpius/tools/fit_stripe.py).
// Measured on the reference:
//   - the light does not move with the music in detail: a fixed glow (a bell along the stripe, brightest a little right
//     of its middle) and particles that twinkle in place (lag-1 frame correlation 0.7): dots and short horizontal
//     dashes in white, amber, orange, red, pink, violet, blue and cyan, dense in a band through the middle (three bright
//     lanes about 2.5 px apart) and thinning out up, down and towards the ends; now and then a bright vertical flare
//     that swells and shrinks within ~0.15 s while sliding sideways (~140 px/s);
//   - its brightness follows the input's peak level: dark below -67.5 dBFS, full from -40 dBFS up, linear in dB between
//     (0.27 at -60). The level it follows rises at once and falls at 50 dB/s, so after a loud stop the light stays full
//     for a while (0.6 s after -10 dBFS), then fades out within ~0.55 s; after a quiet one it fades at once;
//   - with the particle animation off (settings), the stripe stays dark.
// The light is added (as light), computed per physical pixel and drawn as one image per frame (60 frames/s).
namespace sc
{
class Stripe : public juce::Component, private juce::Timer
{
public:
    Stripe();
    void paint (juce::Graphics&) override;
    // the input's peak level (dBFS) on the editor's clock; whether audio runs; whether the animation is on
    void setInput (float peakDb, bool running, bool animated);
    // the snapshot tool: one still frame at a brightness (0 ... 1)
    void freeze (float level);
    // paints what is under the stripe (in the editor's design coordinates), drawn once per scale
    std::function<void (juce::Graphics&)> paintBackground;

    // bounds in design px (the tube's inside)
    static constexpr float left = 10.0f, top = 272.0f, width = 868.0f, height = 36.0f;
    static constexpr double releaseDbPerSec = 50.0;
    static float brightness (float db) { return juce::jlimit (0.0f, 1.0f, (db + 67.5f) / 27.5f); }

private:
    enum class Kind { dot, dash, faint, blob, flare };
    struct Particle
    {
        Kind kind;
        float x, y, vx, vy, w, h, r, g, b, level, born, life, flicker, rate;
    };
    void timerCallback() override;
    void advance (double now);
    void spawn (Particle&, double now, bool anyAge);
    void render (int widthPx, int heightPx, float pxPerDesign);
    void renderGlow (int widthPx, int heightPx, float pxPerDesign);
    void splat (const Particle&, float level, float pxPerDesign);

    juce::Random rng { 1234 };
    std::vector<Particle> particles;
    float envelope = 0.0f, peak = -200.0f, detector = -200.0f;
    double lastTime = 0.0, clock = 0.0;
    bool animated = true, frozen = false;
    std::vector<float> light, glow, under;   // RGB per physical pixel: this frame's light, the glow at full brightness,
                                             // what is under the stripe
    int bufW = 0, bufH = 0;
    float glowScale = 0.0f;
    juce::Image image, background;
};
}
