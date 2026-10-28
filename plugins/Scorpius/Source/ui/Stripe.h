#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

// The visualiser stripe's light, over the dark tube the editor paints: a glow and a field of small coloured particles,
// as the reference shows while audio plays (filmed at 200 % with RefCapture, plugins/Scorpius/tools/fit_stripe.py).
// Measured on the reference:
//   - the light does not move with the music in detail: a fixed glow (a bell along the stripe, brightest a little right
//     of its middle) and particles: dots, round blobs and short horizontal dashes in amber, orange-red, azure, blue-
//     violet, cyan and white, dense in a band through the middle (three bright lanes about 2.5 px apart) where most
//     twinkle in place, thinning out up, down and towards the ends; above the band they stream outwards from the
//     middle as if coming at you (sideways at ~0.45 times their distance from it per second, rising 10 ... 40 px/s);
//     ~3 tall flares at any time (bright capsules up to ~20 px high, white, violet, red, cyan, yellow) swelling and
//     shrinking within ~0.15 s, barely sliding; and the band is reflected in the glass's lower rim (a line of
//     its light, strongest towards the ends) (UI round 8: films of music, particle tracking);
//   - its brightness follows a level the processor keeps (dsp/Light.h: the input's peak, taken when it jumps and when
//     the sound stops, falling at 50 dB/s): dark below -72 dBFS, full from -40 dBFS up, ((dB + 72) / 32)^1.25 between
//     (measured with tones from -65 to -40 dBFS: 0.16, 0.29, 0.44, 0.61, 0.81, 1), and eased over ~30 ms on screen;
//   - with the particle animation off (settings), the stripe stays dark.
// Our own style (settings, "SCORPIUS"): green venom shot at you, through a pane of glass. Far away in the middle beats a
// crimson heart (Antares, the scorpion's red star) shading into a green haze; out of it comes venom (acid lime through
// the controls' mint to deep teal) flying in 3-D towards the viewer, seen with depth of field: far drops small and
// soft, those in focus sharp and streaking, the nearest big out-of-focus discs. Quiet music: a slow drip; louder: a
// steady spray; each beat a strike (a burst, a ring, globs, and a pressure wave from the heart). Drops that reach the
// glass behave as water on a window: they hit flat and spread (a thin uneven film, a splash ring, droplets flung off),
// pull back into beads, and the beads are lenses: each shows the scene behind it upside down and small, darker at its
// rim, with a bright caustic at its foot and a glint. Beads that touch merge; once heavy enough they slide down in fits
// and starts, sweeping up the beads in their way and leaving a wet track, and they shrink as they dry. The glass itself:
// a faint reflection drifting across it, darker edges, and dust and fine scratches that catch the light of the venom.
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
    // ours: the low and mid bands' levels (linear, as the circles read them: Visuals.h), for the venom
    void setBands (float lows, float mids);
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
    // ours: the venom, in 3-D (x and y across, z the distance: 1 far, 0 at the glass), or on the glass
    struct Drop
    {
        enum class Type { spray, glob, mist, bead, track, ring, flash, wave } type;
        float X, Y, Z, vX, vY, vZ, size, r, g, b, level, born, life, seed, sx, sy, px, py;
        bool wet;                    // a spray drop that lands on the glass (the rest fly past)
        float slideAt, travel;       // a bead: when it started to run down (0: not yet), how far since its last track
        float spread;                // a bead: how flat it still is from its impact (1 just hit ... 0 a round bead)
    };
    void fire (bool strike);
    void emit (Drop::Type, float depth, float speed, float size, float level, float angle = -1.0f);
    Drop onGlass (const Drop& from, Drop::Type, float x, float y, float size, float life, float delay);
    void land (const Drop&);
    void hitGlass (const Drop&);
    void advanceVenom (float dt);
    void coalesce();
    void addDrop (const Drop&);
    void renderVenom (float pxPerDesign);
    void renderGland (float pxPerDesign);
    void renderGlass (float pxPerDesign);
    void lens (const Drop&, float pxPerDesign);
    void wetTrack (const Drop&, float pxPerDesign);
    void prepareGlass (int widthPx, int heightPx, float pxPerDesign);
    void droplet (float x, float y, float dx, float dy, float size, float level, const Drop&, float pxPerDesign);
    void splatLine (float x0, float y0, float x1, float y1, float width, float level, float r, float g, float b, float pxPerDesign);
    void splatGlow (float x, float y, float rx, float ry, float level, float r, float g, float b, float pxPerDesign);
    void splatDisc (float x, float y, float radius, float level, float r, float g, float b, float pxPerDesign);
    std::vector<Drop> drops;
    float sprayDue = 0.0f, globDue = 0.0f, mistDue = 0.0f, charge = 0.0f, squeeze = 0.0f, bass = 0.0f, mids = 0.0f;
    double lastBurst = -1.0, lastHeartbeat = 0.0;
    // ours: splats add to the canvas (the light by default); the scene behind the glass (the light over what is under
    // it), and the glass's layers: how much the drops on it cover (one channel), what they show (the scene through
    // them), glints and reflections on top, how much darker its edges are, its dust and scratches (one channel)
    float* canvas = nullptr;
    std::vector<float> scene, cover, front, gloss, vignette, dust;
    std::vector<int> gridHead, gridNext;
    int glassW = 0, glassH = 0;
    struct Particle
    {
        Kind kind;
        float x, y, vx, vy, w, h, r, g, b, level, born, life, flicker, rate;
        float expand;                // how fast it streams outwards from the middle (per second; 0: it does not)
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
