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
// Our own style (settings, "SCORPIUS"): two scorpions drawn in green light in the middle of the tube, back to back
// and mirrored, side on, their tails arched over their backs towards each other. Each has plates along its back, a
// keel and grains on each tail segment, a bulb and a curved blade for its sting, jointed legs, and heavy pincers
// with toothed fingers, resting on the ground. Their outlines are neon (a halo, a glow and a thin core, green on the
// body and cyan- mint towards the claws and the sting) with sparks running along them, a faint glow fills their
// bodies, and the whole of their light blooms as neon does in a photograph. They sway and breathe; on every kick
// (dsp/Kick.h) their tails strike forwards over their heads with a pulse of light running up them. They shed green
// motes all the time, more the louder the music and on each beat, which drift off them, flow round them rather than
// across their lines, and stream out to the black holes past the ends as the ORION style's particles do (the same
// pull, funnel and thinning). Their light is reflected in the glass's lower rim. When the transport stops they fade
// as fast as the ORION style does. With no sound they stay faintly lit and still, shedding nothing; with the
// particle animation off the stripe is dark.
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
    // the low and mid bands' levels (linear, as the circles read them: Visuals.h)
    void setBands (float lows, float mids);
    // ours: a kick (dsp/Kick.h), for the scorpions' tails
    void kick();
    // ours: whether the host's transport runs (and it calls the processor): stopped, the scorpions' light fades as
    // fast as the ORION style's does
    void setPlaying (bool);
    // ours: the project's tempo (BPM; 0 when the host gives none): the ORION style's particles move at their pace times
    // the beats per second (1 ... 3.5 times; 2 times without a tempo), the scorpion's motes at three quarters of that
    // and the scorpion itself at half; easing to a new tempo over ~0.5 s
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
    void splatLine (float x0, float y0, float x1, float y1, float width, float level, float r, float g, float b, float pxPerDesign);
    void splatGlow (float x, float y, float rx, float ry, float level, float r, float g, float b, float pxPerDesign);

    // ours: the scorpion, as strokes of light (design coordinates), rebuilt each frame from its pose
    enum class Part { body, detail, tail, telson, sting, arm, claw, finger, leg };
    struct Stroke
    {
        std::vector<juce::Point<float>> pts;
        float width, bright;
        Part part;
        float tint;                  // 0 its green ... 1 the cyan-mint of its tips
        std::vector<float> along;    // the distance along it at each point
    };
    struct Dot { juce::Point<float> p; float r, level, tint; };              // an eye, a grain, a joint
    struct Fill { juce::Point<float> p; float rx, ry, level; };            // the faint glow inside its body
    // a mote it sheds: drifting off it, then streaming out to the black holes
    struct Mote { float x, y, vx, vy, age, rise, expand, size, twinkle, r, g, b, level; float side; };
    struct Runner { size_t stroke; float at, speed, bright; };
    void buildScorpion (float strike, float time);
    void advanceScorpion (float real, float paced);
    void renderScorpion (float pxPerDesign);
    void strokeNeon (const Stroke&, float level, float pxPerDesign);
    // where the scorpions are: a grid of design pixels around them, 1 on their lines and inside their bodies falling to
    // 0 ~2.5 px out, so the motes keep off them (born outside, pushed round, faded where they still cross)
    void mapScorpions();
    float onScorpion (float x, float y) const;
    juce::Point<float> awayFromScorpion (float x, float y) const;
    std::vector<float> scorpionMap;
    static constexpr float mapLeft = 300.0f, mapTop = 270.0f;
    static constexpr int mapW = 290, mapH = 40;
    juce::Point<float> pointAlong (const Stroke&, float distance) const;
    std::vector<Stroke> strokes;
    std::vector<Dot> glints;
    std::vector<Fill> fills;
    std::vector<juce::Point<float>> spines[2];                               // the tails', from the body to the sting
    std::vector<Mote> motes;
    std::vector<Runner> runners;
    juce::Point<float> stingTips[2], pincers[4];                             // (each scorpion's; two pincers each)
    // strike: 1 on a kick, running down; struck: the tails' swing (0 ... 1), from strikeFrom when a kick came mid-swing
    // presence: the light's level, rising with it at once but fading over ~1.5 s while the transport runs, for the
    // scorpions' glow and their shedding (so at a slow tempo they do not go out between the hits); stopped, the light's
    // level itself (fading as the ORION style's)
    float strike = 0.0f, struck = 0.0f, strikeFrom = 0.0f, flash = 0.0f, moteDue = 0.0f, bodyTime = 0.0f, presence = 0.0f;
    bool kicked = false, playing = true;
    // the scorpion's light before it blooms, and the blurs it blooms with
    std::vector<float> layer, region, halfRes, blurred, midBlur, blurTmp, nearest;

    float* canvas = nullptr;
    juce::Random rng { 1234 };
    std::vector<Particle> particles;
    float envelope = 0.0f, levelIn = -200.0f, bass = 0.0f, mids = 0.0f;
    double lastTime = 0.0, elapsed = 0.0;
    float pace = 2.0f, paceTarget = 2.0f;
    bool animated = true, frozen = false, moving = false, beat = false;
    Style style = Style::orion;
    std::vector<float> light, glow, under, scene;     // RGB per physical pixel: this frame's light, the glow at full
                                                      // brightness, what is under it, the two added up
    int bufW = 0, bufH = 0;
    float glowScale = 0.0f;
    juce::Image image, background;
};
}
