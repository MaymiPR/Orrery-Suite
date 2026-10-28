#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace ap
{
// The centre "particle" visualiser, measured on the reference (RESEARCH.md "Visualiser"):
//  - a black glass racetrack around the amount pill, drawn from the measured lighting field (GlassModel.h);
//  - while audio plays, a swarm of short-lived sparks swirls round the ring: a free-running 1 s loop at 30 frames/s
//    (the reference's sprite repeats every second and does not restart with the audio), ~290 sparks on screen,
//    drifting counter-clockwise at ~95 px/s with a large random scatter (median speed ~160 px/s), lifetimes mostly
//    under 0.2 s, the outer ones pulled inwards;
//  - driven by the input's level (after the input gain): a peak meter that jumps up at once and falls 90 dB/s. Full
//    brightness down to -40 dBFS, dark at -75 (measured with sine and noise staircases, ramps and stops from five
//    levels): with v = (meter + 75) / 35, the glow is v^1.6 and the sparks ((v - 0.4) / 0.6)^0.7. So after a stop
//    from -6 dBFS the light holds ~0.4 s and fades in ~0.4 s, and a quiet ending fades with the music. White at
//    amount 0 % or with the character section bypassed, blending linearly to full colour at 100 %.
//  - our own "antiparticle" style (settings page): bubble-chamber pair creation and annihilation, a 3 s loop.
// Ours, in both styles: the animation runs at the project's tempo (its pace times tempo / 120: 1 time at 120 BPM,
// 0.5 ... 1.75 times, 1 time without a tempo), drawn at 60 frames/s. And in the god particle style the sparks glow
// like neon, their light added up as light is (a core a little whiter than its colour in a halo of the pure colour and
// a wide faint bloom, burning whiter where they crowd; glowing up
// and dying away over ~0.1 ... 0.9 s of a 4 s loop) and the amount pill is a black hole: the sparks near it are drawn
// in, faster the closer, spiralling with the flow, thinner as they go, and gone at its edge, where a thin ring of bent
// light turns with them.
// Drawn in editor design coordinates (the component covers the centre column).
class Visualizer : public juce::Component, private juce::Timer
{
public:
    Visualizer();
    void paint (juce::Graphics&) override;

    // called at 30 Hz by the editor: the input's sample peak this frame (dBFS; -200 when no audio is processed), colour
    // mix 0 (white / ash) .. 1 (colour), animation on/off, and the style: the god particle (the reference's sparks) or
    // the antiparticle (ours)
    void advance (float inputPeakDb, float colourMix, bool animate, bool antiparticle = false);
    // the project's tempo (BPM; 0 when the host gives none)
    void setTempo (float bpm);

    // racetrack geometry (editor design px): the glass, and the amount pill on it (not quite concentric: measured)
    static constexpr float cx = 367.75f, cy = 148.25f, halfLength = 75.25f;
    static constexpr float pillRadius = 50.5f, outerRadius = 113.0f;           // sparks stay between these
    static constexpr float pillCx = 368.1f, pillCy = 148.9f, pillHalfLength = 74.7f, pillR = 49.75f;

private:
    struct Spark { float birth, life, size, bright; int hue, first, count; };
    std::vector<Spark> sparks;
    std::vector<juce::Point<float>> tracks;      // each spark's path, every 1/120 s from its birth (Spark::first on)
    static constexpr float godLoop = 4.0f, trackStep = 1.0f / 120.0f;
    std::vector<float> lightBuf;                 // the sparks' light, RGB per physical pixel (added up as light)
    juce::Image lightImg;
    // antiparticle: as in a bubble chamber. A gamma ray turns into a particle / antiparticle pair whose tracks curl
    // away in opposite directions (opposite charge in a magnetic field) and spiral in as they lose energy; elsewhere
    // pairs meet and annihilate in a flash that sends two gamma rays apart. Matter cyan, antimatter magenta.
    struct Pair { float birth, x, y, dir, speed, omega, tau, life; };         // pair creation at (x, y)
    struct Annihilation { float birth, x, y, dir, approach, curl; };          // meeting point, gamma direction
    struct Speck { float birth, life, x, y, vx, vy, size; bool anti; };     // the matter / antimatter counter-flow
    std::vector<Pair> pairs;
    std::vector<Annihilation> annihilations;
    std::vector<Speck> specks;
    bool antiStyle = false;
    void paintGodly (juce::Graphics&);
    void paintAntiparticle (juce::Graphics&);
    float level = 0.0f;           // the sparks' visibility 0 .. 1
    float glow = 0.0f;            // the lit glass's visibility 0 .. 1 (fades slower than the sparks)
    float meterDb = -200.0f;      // the input peak meter that drives both (dBFS)
    double lastAdvance = 0.0;     // when advance() last ran (s)
    float colourMix = 1.0f;
    double clock = 0.0;           // free-running animation clock (s, at the tempo's pace)
    float pace = 1.0f, paceTarget = 1.0f;
    bool animating = false;
    void timerCallback() override;
    juce::Image frame, lit, litGrey, litViolet;  // the glass unlit / lit (colour, white, antiparticle violet), per size
    float framePx = 0.0f;
    void renderFrame (float scale);
};
}
