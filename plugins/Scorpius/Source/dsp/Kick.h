#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

// The kicks the SCORPIUS particle style's scorpions strike on (ours, not the reference's), found on the audio thread in
// the input after the input gain and looked at every 2.5 ms, in two bands:
//   - the sub (two low-passes at 90 Hz), its peak over the last 25 ms (steady through a 20 Hz cycle) against a slow
//     level that rises over 60 ms and follows a decay within 15 ms: a kick when the peak is 3 dB above it and above
//     -28 dBFS (a snare's body, leaking in, stays below). This finds a kick or an 808 coming out of silence or out of what the last one has decayed to;
//   - the punch (a high-pass at 70 Hz and a low-pass at 200 Hz), its peak over 10 ms against a slow level (30 ms up,
//     8 ms down): an attack 4 dB above it and above -30 dBFS is a kick if the sub sounds too (above -26 dBFS) within
//     20 ms. This finds a kick landing on one still ringing, whose sub hardly rises; a snare, with no sub, never counts.
// Either way the sub must be at least half as strong as the punch (a snare, its body around 200 Hz, is far from it).
// After a kick, for 60 ms, both slow levels follow the peaks straight up (the rest of its attack is not another kick);
// so a held 808 counts once, and there is at most one kick every 60 ms.
// The editor reads the count on its own clock.
namespace sc
{
class Kick
{
public:
    void prepare (double sampleRate)
    {
        lowPass (sub[0], 90.0, sampleRate); sub[1] = sub[0];
        highPass (punch[0], 70.0, sampleRate); lowPass (punch[1], 200.0, sampleRate);
        chunk = juce::jmax (1, (int) std::lround (0.0025 * sampleRate));
        const double ms = 1000.0 * (double) chunk / sampleRate;
        subUp = std::exp (-ms / 60.0); subDown = std::exp (-ms / 15.0);
        punchUp = std::exp (-ms / 30.0); punchDown = std::exp (-ms / 8.0);
        holdChunks = (int) std::ceil (60.0 / ms);
        confirmChunks = (int) std::ceil (20.0 / ms);
        reset();
    }
    void reset()
    {
        for (auto* f : { &sub[0], &sub[1], &punch[0], &punch[1] }) f->z1 = f->z2 = 0.0;
        subPeaks.fill (0.0); punchPeaks.fill (0.0);
        subPos = punchPos = count = 0;
        subPeak = punchPeak = subSlow = punchSlow = 0.0;
        sinceKick = holdChunks; pending = 0;
    }
    void process (const juce::AudioBuffer<float>& in)
    {
        const int n = in.getNumSamples(), channels = juce::jmin (2, in.getNumChannels());
        if (channels == 0) return;
        for (int i = 0; i < n; ++i)
        {
            double x = 0.0;
            for (int c = 0; c < channels; ++c) x += in.getSample (c, i);
            x /= (double) channels;
            subPeak = juce::jmax (subPeak, std::abs (sub[1].run (sub[0].run (x))));
            punchPeak = juce::jmax (punchPeak, std::abs (punch[1].run (punch[0].run (x))));
            if (++count == chunk) endChunk();
        }
    }
    int kickCount() const { return kicks.load(); }

private:
    struct Biquad
    {
        double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    // (Butterworth, by the bilinear transform)
    static void lowPass (Biquad& f, double hz, double fs) { design (f, hz, fs, false); }
    static void highPass (Biquad& f, double hz, double fs) { design (f, hz, fs, true); }
    static void design (Biquad& f, double hz, double fs, bool high)
    {
        const double w = std::tan (juce::MathConstants<double>::pi * hz / fs), r2 = juce::MathConstants<double>::sqrt2, k = 1.0 / (1.0 + r2 * w + w * w);
        f.b0 = high ? k : w * w * k; f.b1 = high ? -2.0 * k : 2.0 * w * w * k; f.b2 = f.b0;
        f.a1 = 2.0 * (w * w - 1.0) * k; f.a2 = (1.0 - r2 * w + w * w) * k;
    }
    static double gain (double db) { return std::pow (10.0, db / 20.0); }

    void endChunk()
    {
        static const double subRise = gain (3.0), subFloor = gain (-28.0), punchRise = gain (4.0), punchFloor = gain (-30.0), subGate = gain (-26.0);
        subPeaks[(size_t) subPos] = subPeak; subPos = (subPos + 1) % (int) subPeaks.size();
        punchPeaks[(size_t) punchPos] = punchPeak; punchPos = (punchPos + 1) % (int) punchPeaks.size();
        subPeak = punchPeak = 0.0; count = 0;
        double subHeld = 0.0, punchHeld = 0.0;
        for (auto p : subPeaks) subHeld = juce::jmax (subHeld, p);
        for (auto p : punchPeaks) punchHeld = juce::jmax (punchHeld, p);
        ++sinceKick;
        bool kick = false;
        // (a kick's or an 808's sub is about as strong as its punch or stronger; a snare's is far weaker)
        const bool bassy = subHeld > 0.5 * punchHeld;
        if (sinceKick >= holdChunks)
        {
            if (subHeld > subFloor && subHeld > subSlow * subRise && bassy) kick = true;
            if (punchHeld > punchFloor && punchHeld > punchSlow * punchRise) pending = confirmChunks;
            if (pending > 0 && subHeld > subGate && bassy) kick = true;
        }
        if (pending > 0) --pending;
        if (kick)
        {
            kicks.fetch_add (1);
            sinceKick = 0; pending = 0;
            subSlow = subHeld; punchSlow = punchHeld;
        }
        subSlow = subHeld + (subSlow - subHeld) * (subHeld > subSlow ? subUp : subDown);
        punchSlow = punchHeld + (punchSlow - punchHeld) * (punchHeld > punchSlow ? punchUp : punchDown);
        if (sinceKick < holdChunks) { subSlow = juce::jmax (subSlow, subHeld); punchSlow = juce::jmax (punchSlow, punchHeld); }
    }

    Biquad sub[2], punch[2];
    std::array<double, 10> subPeaks {};        // (25 ms of 2.5 ms chunks)
    std::array<double, 4> punchPeaks {};       // (10 ms)
    int subPos = 0, punchPos = 0, count = 0, chunk = 120, holdChunks = 24, confirmChunks = 8, sinceKick = 24, pending = 0;
    double subPeak = 0, punchPeak = 0, subSlow = 0, punchSlow = 0, subUp = 0, subDown = 0, punchUp = 0, punchDown = 0;
    std::atomic<int> kicks { 0 };
};
}
