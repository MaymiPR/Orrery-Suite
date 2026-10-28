#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

// The visualiser stripe's light, over the dark tube the editor paints. ORION style: ours after the reference (its
// films measured with tones and music, plugins/Scorpius/tools/fit_stripe.py, RESEARCH.md UI rounds 3, 4, 6, 8, 9 and 10),
// a heavenly field of light filling the tube from end to end:
//   - starlike sparks, white-hot at the core in coloured halos (gold, amber, rose, ice blue, cyan, azure, lavender, in
//     the reference's balance of hues), and short streaks of light in three lanes through the middle; like the
//     reference's, they are scattered afresh every frame (from one frame of its film to the next its particles keep no
//     more of their places than chance), so the field shimmers;
//   - streamers flowing out of the middle as if coming at you: outwards at ~30 px/s plus ~0.4 times their distance from
//     the middle per second, rising above the band and sinking below it (the reference's, tracked frame by frame);
//   - pillars of light (~5 at a time, each a frame or two, as the reference's flares) with soft halos;
//   - ours: now and then a star that glints with long thin rays, and a few slow, soft orbs drifting in the haze;
//   - a fixed glow (fitted to the reference) with a faint violet haze above the band;
//   - ours: a black hole past each end of the tube, off screen: what flows out of the middle is drawn in, faster the
//     closer, funnelled to the middle's height and stretched, dimming into the dark at the ends and gone;
//   - the glass: the band's light reflected in its lower rim, two dotted lines of reflected light (y 282.25 and 304,
//     segments ending at the same places on both, closer together to the right) and bright spots at the ends of the
//     lower one;
//   - its brightness follows a level the processor keeps (dsp/Light.h: the input's peak, taken when it jumps and when
//     the sound stops, falling at 50 dB/s): dark below -72 dBFS, full from -40 dBFS up, ((dB + 72) / 32)^1.25 between,
//     eased over ~30 ms on screen; with the particle animation off (settings), the stripe stays dark.
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
    // ours: the project's tempo (BPM; 0 when the host gives none): the particles (both styles) move at their pace times
    // the beats per second (1 ... 3.5 times; 2 times without a tempo), easing to a new one over ~0.5 s
    void setTempo (float bpm);
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
    void addReflections (int widthPx, int heightPx, float pxPerDesign);
    void splat (const Particle&, float level, float pxPerDesign);
    void splatStar (const Particle&, float level, float pxPerDesign);
    void heavenly (const Particle&, float level, float age, float pxPerDesign);

    juce::Random rng { 1234 };
    std::vector<Particle> particles;
    float envelope = 0.0f, levelIn = -200.0f, heart = 0.0f;
    double lastTime = 0.0, elapsed = 0.0;
    float pace = 2.0f, paceTarget = 2.0f;
    bool animated = true, frozen = false, moving = false, beat = false;
    Style style = Style::orion;
    std::vector<float> light, glow, glowHeart, under;   // RGB per physical pixel: this frame's light, the glow at full
                                                        // brightness (and the heart's beat, ours), what is under it
    int bufW = 0, bufH = 0;
    float glowScale = 0.0f;
    juce::Image image, background;
};
}
