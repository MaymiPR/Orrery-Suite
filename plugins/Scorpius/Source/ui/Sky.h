#pragma once
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <vector>

// Our own particle style for the stripe (settings page, "SCORPIUS"; the other is the reference's): the constellation
// the plug-in is named after, as the night sky shows it (east to the left: the head on the right, the curled tail and
// its stinger on the left, squeezed into the stripe), played by the music:
//   - an aurora of the spectrum spreads both ways from the red heart Antares: the lows next to it in its own crimson,
//     then magenta, violet, blue, cyan and mint out to the highs in gold at the ends; each part swells with its band,
//     waves, shimmers in rays and flares when its band hits;
//   - a kick makes Antares beat: it swells, sends a ring out over the sky and a pulse of light along the body;
//   - a snare shoots venom from the head along the body to the stinger, which bursts into coloured sparks;
//   - the hi-hats make the dust stars glint in colour; each main star brightens and flashes with its own band;
//   - the Milky Way lies warm across the tail and breathes with the lows.
// It all follows the same brightness as the reference's style (Stripe.h), so the light comes and goes the same way.
namespace sc
{
class Sky
{
public:
    static constexpr int numBands = 24;      // the spectrum (dsp/Visuals.h): log-spaced, 30 Hz ... 16 kHz

    Sky();
    // a frame: the time step, the stripe's brightness (0 ... 1), whether the input just jumped up (a beat), and the
    // bands' levels (linear, a sine reads its amplitude; nullptr when there are none)
    void advance (float seconds, float brightness, bool beat, const float* bands);
    // adds the light to an RGB buffer (w x h physical pixels, s pixels per design px, origin at the stripe's corner)
    void render (std::vector<float>& light, int w, int h, float s, float brightness);

    struct Star { float x, y, size, r, g, b, phase, rate; };
private:
    struct Rgb { float r, g, b; };
    static Rgb palette (float u);                         // the spectrum's colours, 0 (lows) ... 1 (highs)
    float bandPosition (float x) const;                   // where x lies in the spectrum (0 ... 1), out from the heart
    float levelAt (float u) const;                        // the spectrum's level there (0 ... 1)
    float flashAt (float u) const;
    void addDot (std::vector<float>&, int w, int h, float s, float x, float y, float radius, float halo, float haloShare,
                 Rgb, float level) const;
    void addPoint (std::vector<float>&, int w, int h, float s, float x, float y, Rgb, float level) const;
    void addStreak (std::vector<float>&, int w, int h, float s, juce::Point<float> a, juce::Point<float> b, Rgb, float atA, float atB) const;
    void addRing (std::vector<float>&, int w, int h, float s, juce::Point<float> c, float rx, float ry, float width, Rgb, float level) const;
    void renderAurora (std::vector<float>&, int w, int h, float s, float brightness);
    juce::Point<float> along (float distance) const;      // a point on the body, measured from the head
    void listen (float dt, const float* bands, bool beat);
    void kick (float strength), snare (float strength), hat (float strength);

    std::vector<Star> stars, dust;
    std::vector<juce::Point<float>> body;                 // the head ... the stinger
    std::vector<float> bodyLength;                        // distance from the head to each point
    float heartX = 0.0f, heartAlong = 0.0f;

    // the music: each band's level and its own flash; three detectors of hits (kick, snare, hi-hat)
    struct Band { float db = -120.0f, avg = -120.0f, ref = -60.0f, level = 0.0f, flash = 0.0f; };
    std::array<Band, numBands> band;
    struct Hits { int from, to; float db = -120.0f, avg = -120.0f, prev = -120.0f, hold = 0.0f, level = 0.0f; };
    Hits kicks { 0, 6 }, snares { 9, 19 }, hats { 20, 24 };
    float loudness = 0.0f;

    struct Ring { float age, life, strength; };
    struct Pulse { float age, life, strength; };
    struct Comet { float along, speed, strength; };
    struct Spark { float x, y, vx, vy, age, life; Rgb c; };
    struct Glint { int star; float age, life; Rgb c; };
    std::vector<Ring> rings;
    std::vector<Pulse> pulses;
    std::vector<Comet> comets;
    std::vector<Spark> sparks;
    std::vector<Glint> glints;
    juce::Random rng { 5150 };
    float time = 0.0f, heart = 0.0f, sting = 0.0f, cometDebt = 0.0f, glintDebt = 0.0f;
    std::vector<float> haze;                              // the Milky Way at the current size (RGB, full brightness)
    int hazeW = 0, hazeH = 0;
    float hazeS = 0.0f;
};
}
