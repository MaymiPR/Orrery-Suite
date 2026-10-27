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
// Our own style (settings, "SCORPIUS"): venom coming at you. A venom gland glows in the middle of the stripe, far
// away, and sprays venom out of the screen: each drop flies towards the viewer, slow and small in the distance,
// growing, brightening and speeding outwards as it comes near (a warp field), streaking, until it flies past the
// stripe's edges. It sprays all the time (more the louder it is); each beat (the light's jump up) fires a burst: a
// flood of drops, a ring that spreads as it comes, and big globs that splat on the glass and run down it. Colourful:
// acid green, teal, electric violet, magenta and amber, each burst in its own, the set drifting round the hue circle
// about once a minute; the haze behind is tinted to match and flares at the middle on a beat.
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
    // ours: the venom, in 3-D (x and y across, z the distance: 1 far, 0 at the glass), or on the glass (splats, drips)
    struct Drop
    {
        enum class Type { spray, glob, splat, drip, flash } type;
        float X, Y, Z, vX, vY, vZ, size, r, g, b, level, born, life, hue, sx, sy, px, py;
    };
    void fire (bool burst);
    void emit (Drop::Type, float hue, float speed, float size, float level, float angle = -1.0f);
    void advanceVenom (float dt);
    void addDrop (const Drop&);
    void renderVenom (float pxPerDesign);
    void renderGland (float pxPerDesign);
    void splatLine (float x0, float y0, float x1, float y1, float width, float level, float r, float g, float b, float pxPerDesign);
    std::vector<Drop> drops;
    float hueDrift = 0.0f, sprayDue = 0.0f, globDue = 0.0f, charge = 0.0f;
    double lastBurst = -1.0;
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
