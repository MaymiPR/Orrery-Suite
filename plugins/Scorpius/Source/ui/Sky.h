#pragma once
#include <juce_graphics/juce_graphics.h>
#include <vector>

// Our own particle style for the stripe (settings page, "SCORPIUS"; the other is the reference's): the constellation
// the plug-in is named after, as the night sky shows it (east to the left: the head on the right, the curled tail and
// its stinger on the left, squeezed into the stripe). Its stars twinkle, the red heart Antares swells with each beat,
// faint lines join the stars, the tail lies in a warm band of the Milky Way with dust stars, and sparks of venom run
// along the body from the head to the stinger and burst from it when the music hits. It follows the same brightness
// as the reference's style (Stripe.h), so the light comes and goes the same way.
namespace sc
{
class Sky
{
public:
    Sky();
    // a frame: the time step, the stripe's brightness (0 ... 1) and whether the input just jumped up (a beat)
    void advance (float seconds, float brightness, bool beat);
    // adds the light to an RGB buffer (w x h physical pixels, s pixels per design px, origin at the stripe's corner)
    void render (std::vector<float>& light, int w, int h, float s, float brightness);

    struct Star { float x, y, size, r, g, b, phase, rate; };
private:
    void addDot (std::vector<float>&, int w, int h, float s, float x, float y, float radius, float halo, float haloShare,
                 float r, float g, float b, float level) const;
    void addLine (std::vector<float>&, int w, int h, float s, juce::Point<float> a, juce::Point<float> b, float level,
                  float r, float g, float bl) const;
    juce::Point<float> along (float distance) const;      // a point on the body, measured from the head

    std::vector<Star> stars, dust;
    std::vector<juce::Point<float>> body;                 // the head ... the stinger
    std::vector<float> bodyLength;                        // distance from the head to each point
    struct Spark { float x, y, vx, vy, age, life, along; bool venom; };
    std::vector<Spark> sparks;
    juce::Random rng { 5150 };
    float time = 0.0f, heart = 0.0f, spawnDebt = 0.0f;
    std::vector<float> haze;                              // the Milky Way at the current size (RGB, full brightness)
    int hazeW = 0, hazeH = 0;
    float hazeS = 0.0f;
};
}
