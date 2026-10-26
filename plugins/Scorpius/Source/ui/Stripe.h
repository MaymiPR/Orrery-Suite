#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

// The visualiser stripe's light, over the dark tube the editor paints: a glow and a field of small coloured particles,
// as the reference shows while audio plays (filmed at 200 % with RefCapture, plugins/Scorpius/tools/fit_stripe.py).
// Measured on the reference:
//   - the light does not move with the music in detail: a fixed glow (a bell along the stripe, brightest a little right
//     of its middle) and particles that twinkle in place: dots and short horizontal dashes in white, amber, orange,
//     red, pink, violet, blue and cyan, dense in a band through the middle (three bright lanes about 2.5 px apart) and
//     thinning out up, down and towards the ends; now and then a bright vertical flare that swells and shrinks within
//     ~0.15 s while sliding sideways (~140 px/s);
//   - its brightness follows a level the processor keeps (dsp/Light.h: the input's peak, taken when it jumps and when
//     the sound stops, falling at 50 dB/s): dark below -72 dBFS, full from -40 dBFS up, ((dB + 72) / 32)^1.25 between
//     (measured with tones from -65 to -40 dBFS: 0.16, 0.29, 0.44, 0.61, 0.81, 1), and eased over ~30 ms on screen;
//   - with the particle animation off (settings), the stripe stays dark.
// Our own style (settings, "SCORPIUS") is the same field with the same light, in the plug-in's own colours: the glow
// and the particles burn crimson and amber in the middle, Antares the scorpion's red heart, and cool to the mint of
// the controls, teal and ice blue towards the ends; its flares are four-pointed stars and some particles twinkle as
// small stars; the heart of the glow beats when the music jumps up.
// The light is added (as light), computed per physical pixel and drawn as one image per frame of the editor's clock.
namespace sc
{
class Stripe : public juce::Component
{
public:
    Stripe();
    void paint (juce::Graphics&) override;
    // on the editor's clock: the light's level (dBFS, dsp/Light.h), whether it jumped up since the last frame (a
    // beat), whether the animation is on
    void setInput (float levelDb, bool beat, bool animated);
    // the snapshot tool: one still frame at a brightness (0 ... 1)
    void freeze (float level);
    // the reference's particles, or ours
    enum class Style { orion, scorpius };
    void setStyle (Style);
    // paints what is under the stripe (in the editor's design coordinates), drawn once per scale
    std::function<void (juce::Graphics&)> paintBackground;
    // the clock (seconds; the snapshot tool's films run on their own)
    std::function<double()> clock = [] { return juce::Time::getMillisecondCounterHiRes() * 0.001; };

    // bounds in design px (the tube's inside)
    static constexpr float left = 10.0f, top = 272.0f, width = 868.0f, height = 36.0f;
    static constexpr double easing = 0.03;
    static float brightness (float db) { return std::pow (juce::jlimit (0.0f, 1.0f, (db + 72.0f) / 32.0f), 1.25f); }

private:
    enum class Kind { dot, dash, faint, blob, flare, star };
    struct Particle
    {
        Kind kind;
        float x, y, vx, vy, w, h, r, g, b, level, born, life, flicker, rate;
    };
    void advance (double now);
    void spawn (Particle&, double now, bool anyAge);
    void render (int widthPx, int heightPx, float pxPerDesign);
    void renderGlow (int widthPx, int heightPx, float pxPerDesign);
    void splat (const Particle&, float level, float pxPerDesign);
    void splatStar (const Particle&, float level, float pxPerDesign);

    juce::Random rng { 1234 };
    std::vector<Particle> particles;
    float envelope = 0.0f, levelIn = -200.0f, heart = 0.0f;
    double lastTime = 0.0, elapsed = 0.0;
    bool animated = true, frozen = false, moving = false, beat = false;
    Style style = Style::orion;
    std::vector<float> light, glow, glowHeart, under;   // RGB per physical pixel: this frame's light, the glow at full
                                                        // brightness (and the heart's beat, ours), what is under it
    int bufW = 0, bufH = 0;
    float glowScale = 0.0f;
    juce::Image image, background;
};
}
