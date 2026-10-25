#include "Stripe.h"
#include "StripeGlow.h"
#include "shared/ui/Theme.h"

using namespace juce;
namespace theme = orrery::theme;

namespace sc
{
namespace
{
// the particles' colours and their shares (measured: the hues of the reference's saturated particles; about half of
// its particles read nearly white)
struct Tint { float r, g, b, share; };
constexpr Tint palette[] {
    { 1.00f, 0.96f, 0.94f, 0.20f },     // white
    { 1.00f, 0.90f, 0.78f, 0.12f },     // warm white
    { 1.00f, 0.42f, 0.18f, 0.07f },     // red-orange (hue 15)
    { 1.00f, 0.72f, 0.22f, 0.12f },     // amber (45)
    { 0.95f, 1.00f, 0.32f, 0.02f },     // yellow (75)
    { 0.20f, 0.85f, 1.00f, 0.12f },     // cyan (195)
    { 0.28f, 0.58f, 1.00f, 0.06f },     // azure (225)
    { 0.48f, 0.40f, 1.00f, 0.10f },     // blue-violet (255)
    { 0.78f, 0.38f, 1.00f, 0.01f },     // violet (285)
    { 1.00f, 0.30f, 0.48f, 0.05f } };   // pink-red (345)
constexpr Tint flareTints[] {
    { 1.00f, 0.98f, 0.96f, 0.35f }, { 0.30f, 0.90f, 1.00f, 0.20f }, { 1.00f, 0.90f, 0.70f, 0.15f },
    { 1.00f, 0.75f, 0.30f, 0.12f }, { 1.00f, 0.30f, 0.30f, 0.08f }, { 0.75f, 0.55f, 1.00f, 0.10f } };
constexpr int numParticles = 1700, numFlares = 10;
constexpr float lanes[] { 289.1f, 291.6f, 294.25f, 302.5f };   // the rows the band's dashes keep to (the last one less)
// where the reference's particles are (local maxima per frame, measured): along the stripe in bins of 18.5 design px
// from x 0, and across it in bins of 2 design px from y 272.5
constexpr float alongBins[48] { 0, 1, 3, 7, 12, 15, 18, 22, 26, 31, 36, 37, 39, 42, 43, 40, 42, 42, 47, 48, 50, 45, 49, 47,
                                55, 55, 45, 52, 46, 43, 47, 44, 42, 47, 43, 38, 38, 35, 31, 26, 21, 17, 15, 11, 6, 2, 0, 0 };
constexpr float acrossBins[17] { 31, 16, 21, 23, 28, 31, 47, 103, 240, 241, 209, 161, 124, 100, 88, 45, 15 };
// the same for the dots and blobs alone (the band's dashes are added on top of them)
constexpr float dotBins[17] { 3, 4, 6, 8, 10, 13, 22, 50, 90, 90, 85, 140, 90, 45, 25, 10, 3 };

float gauss (Random& r)
{
    const float u = jmax (1.0e-6f, r.nextFloat()), v = r.nextFloat();
    return std::sqrt (-2.0f * std::log (u)) * std::cos (MathConstants<float>::twoPi * v);
}

template <size_t n>
const Tint& pick (Random& r, const Tint (&tints)[n])
{
    float total = 0.0f;
    for (auto& t : tints) total += t.share;
    float u = r.nextFloat() * total;
    for (auto& t : tints)
        if ((u -= t.share) <= 0.0f) return t;
    return tints[n - 1];
}

float uniform (Random& r, float a, float b) { return a + (b - a) * r.nextFloat(); }

// a value from a histogram (bins of a width from a start), uniform within its bin
template <size_t n>
float fromBins (Random& r, const float (&bins)[n], float start, float width)
{
    float total = 0.0f;
    for (auto b : bins) total += b;
    float u = r.nextFloat() * total;
    for (size_t i = 0; i < n; ++i)
        if ((u -= bins[i]) <= 0.0f) return start + width * ((float) i + r.nextFloat());
    return start + width * (float) n;
}
}

Stripe::Stripe()
{
    setInterceptsMouseClicks (false, false);
    setOpaque (true);           // (it draws what is under it itself, so the editor does not repaint 60 times a second)
    particles.resize (numParticles + numFlares);
    for (size_t i = 0; i < particles.size(); ++i)
    {
        particles[i].kind = i < (size_t) numParticles ? Kind::dot : Kind::flare;
        spawn (particles[i], 0.0, true);
    }
}

void Stripe::spawn (Particle& p, double now, bool anyAge)
{
    if (p.kind == Kind::flare)
    {
        // a flare: a narrow bar that swells to its height and shrinks again while it slides sideways
        const auto& t = pick (rng, flareTints);
        p.r = t.r; p.g = t.g; p.b = t.b;
        p.x = jlimit (40.0f, 840.0f, 455.0f + 190.0f * gauss (rng));
        p.y = 290.5f + 5.0f * gauss (rng);
        p.vx = (rng.nextBool() ? 1.0f : -1.0f) * uniform (rng, 40.0f, 180.0f);
        p.vy = 0.0f;
        p.w = uniform (rng, 2.0f, 3.5f);
        p.h = uniform (rng, 3.5f, 14.0f);
        p.level = uniform (rng, 1.1f, 1.7f);
        p.life = uniform (rng, 0.10f, 0.22f);
        p.born = (float) (now + (anyAge ? uniform (rng, -0.2f, 0.3f) : uniform (rng, 0.0f, 0.15f)));
        p.flicker = 0.0f; p.rate = 0.0f;
        return;
    }
    const auto& t = pick (rng, palette);
    p.r = t.r; p.g = t.g; p.b = t.b;
    p.x = fromBins (rng, alongBins, 0.0f, 18.5f);
    // a dense layer of short dashes through the band (on its lanes or between them), the rest spread across as
    // measured: dots, faint dots and round blobs
    const float u = rng.nextFloat();
    if (u < 0.3f)
    {
        p.kind = Kind::dash;
        const float v = rng.nextFloat();
        p.y = v < 0.45f ? lanes[rng.nextFloat() < 0.92f ? rng.nextInt (3) : 3] + 0.12f * gauss (rng) : uniform (rng, 286.5f, 302.5f);
    }
    else
    {
        const float v = rng.nextFloat();
        p.kind = v < 0.64f ? Kind::dot : v < 0.94f ? Kind::faint : Kind::blob;
        p.y = fromBins (rng, dotBins, 272.5f, 2.0f);
    }
    // moving (measured with frame-to-frame cross-correlation): in the band sideways, either way, at 90 ... 200 px/s;
    // above and below it outwards (rising above, sinking below) and sideways at up to ~100 px/s
    const float off = p.y - 291.0f;
    if (std::abs (off) < 4.5f) { p.vx = (rng.nextBool() ? 1.0f : -1.0f) * uniform (rng, 90.0f, 200.0f); p.vy = 0.0f; }
    else { p.vx = 50.0f * gauss (rng); p.vy = (off > 0.0f ? 1.0f : -1.0f) * uniform (rng, 0.0f, 60.0f); }
    switch (p.kind)
    {
        case Kind::dot:   p.w = uniform (rng, 0.5f, 1.0f); p.h = uniform (rng, 0.5f, 0.8f); p.level = uniform (rng, 0.6f, 1.25f); break;
        case Kind::dash:  p.w = uniform (rng, 1.8f, 5.0f); p.h = uniform (rng, 0.7f, 1.0f); p.level = uniform (rng, 0.5f, 1.0f); break;
        case Kind::faint: p.w = 0.6f; p.h = 0.6f; p.level = uniform (rng, 0.3f, 0.6f); break;
        case Kind::blob:  p.w = uniform (rng, 1.6f, 3.5f); p.h = p.w * uniform (rng, 0.8f, 1.3f); p.level = uniform (rng, 0.6f, 1.1f); break;
        case Kind::flare: break;
    }
    p.life = uniform (rng, 0.05f, 0.3f);
    p.born = (float) (anyAge ? now - uniform (rng, 0.0f, p.life) : now);
    p.flicker = uniform (rng, 0.0f, MathConstants<float>::twoPi);
    p.rate = uniform (rng, 15.0f, 45.0f);
}

void Stripe::setInput (float peakDb, bool running, bool anim)
{
    if (frozen) return;
    animated = anim;
    peak = animated && running ? peakDb : -200.0f;
    if ((brightness (peak) > 0.0f || envelope > 0.0f) && ! isTimerRunning())
    {
        lastTime = Time::getMillisecondCounterHiRes() * 0.001;
        startTimerHz (60);
    }
}

void Stripe::freeze (float b)
{
    frozen = true;
    stopTimer();
    envelope = b;
    // a moment well into the animation, so ages and flares are spread as while it runs
    for (int i = 0; i < 60; ++i) { clock += 1.0 / 60.0; for (auto& p : particles) if (clock - p.born > p.life) spawn (p, clock, false); }
    repaint();
}

void Stripe::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    advance (now);
    repaint();
    if (envelope <= 0.0f && brightness (peak) <= 0.0f) stopTimer();
}

void Stripe::advance (double now)
{
    const double dt = jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;
    clock += dt;
    // the level the light follows: up at once, down at a steady rate (in dB)
    detector = peak >= detector ? peak : jmax (peak, detector - (float) (releaseDbPerSec * dt));
    envelope = animated ? brightness (detector) : 0.0f;
    for (auto& p : particles)
    {
        if (clock - p.born > p.life) spawn (p, clock, false);
        p.x += p.vx * (float) dt;
        p.y += p.vy * (float) dt;
    }
}

void Stripe::renderGlow (int w, int h, float s)
{
    glow.assign ((size_t) (w * h * 3), 0.0f);
    glowScale = s;
    namespace gl = stripeglow;
    for (int j = 0; j < h; ++j)
    {
        const float y = top + ((float) j + 0.5f) / s;
        const float u = (y - gl::rowsFrom) / gl::rowStep;
        const int r0 = jlimit (0, gl::numRows - 1, (int) std::floor (u)), r1 = jmin (gl::numRows - 1, r0 + 1);
        const float f = jlimit (0.0f, 1.0f, u - (float) r0);
        const float row = u < 0.0f || u > (float) (gl::numRows - 1) ? 0.0f : gl::rows[r0] * (1.0f - f) + gl::rows[r1] * f;
        for (int i = 0; i < w; ++i)
        {
            const float x = left + ((float) i + 0.5f) / s;
            for (int c = 0; c < 3; ++c)
            {
                const auto& b = gl::along[c];
                const float d = x - b.c;
                const float v = b.a * std::exp (-(d / b.s) * (d / b.s)) + b.b * std::exp (-std::abs (d) / b.t);
                glow[(size_t) ((j * w + i) * 3 + c)] = row * v / 255.0f;
            }
        }
    }
}

// adds one particle's light: a soft-edged box (its core) and a wider halo
void Stripe::splat (const Particle& p, float level, float s)
{
    const float cx = (p.x - left) * s, cy = (p.y - top) * s;
    float hw = p.w * 0.5f * s, hh = p.h * 0.5f * s;
    if (p.kind == Kind::flare)
    {
        const float u = (float) ((clock - p.born) / p.life);
        hh = p.h * 0.5f * s * std::sin (MathConstants<float>::pi * jlimit (0.0f, 1.0f, u));
    }
    // the small ones (measured on isolated particles): a box with a pixel of soft edge and a faint halo ~1.5 px wide at
    // a tenth of the core, separable (cheap); the big bright ones: a rounded box (a capsule when long, a disc when
    // square) that blooms wider
    const bool big = p.kind == Kind::blob || p.kind == Kind::flare;
    const float edge = jmax (0.4f, 0.3f * s), halo = (big ? 1.5f : 0.6f) * s;
    const float haloShare = big ? 0.25f : 0.07f;
    const float reach = (big ? 3.0f : 2.5f) * halo;
    const int i0 = jmax (0, (int) std::floor (cx - hw - reach)), i1 = jmin (bufW - 1, (int) std::ceil (cx + hw + reach));
    const int j0 = jmax (0, (int) std::floor (cy - hh - reach)), j1 = jmin (bufH - 1, (int) std::ceil (cy + hh + reach));
    if (i0 > i1 || j0 > j1) return;
    if (! big)
    {
        float ex[48], hx[48];
        const int n = jmin (48, i1 - i0 + 1);
        for (int i = 0; i < n; ++i)
        {
            const float d = std::abs ((float) (i0 + i) + 0.5f - cx) - hw;
            ex[i] = d <= 0.0f ? 1.0f : std::exp (-d / edge);
            hx[i] = d <= 0.0f ? 1.0f : std::exp (-d / halo);
        }
        for (int j = j0; j <= j1; ++j)
        {
            const float d = std::abs ((float) j + 0.5f - cy) - hh;
            const float ey = (1.0f - haloShare) * level * (d <= 0.0f ? 1.0f : std::exp (-d / edge));
            const float hy = haloShare * level * (d <= 0.0f ? 1.0f : std::exp (-d / halo));
            float* row = light.data() + (size_t) (j * bufW + i0) * 3;
            for (int i = 0; i < n; ++i, row += 3)
            {
                const float v = ex[i] * ey + hx[i] * hy;
                row[0] += v * p.r; row[1] += v * p.g; row[2] += v * p.b;
            }
        }
        return;
    }
    const float corner = jmin (hw, hh);
    for (int j = j0; j <= j1; ++j)
    {
        const float qy = std::abs ((float) j + 0.5f - cy) - (hh - corner);
        float* row = light.data() + (size_t) (j * bufW + i0) * 3;
        for (int i = i0; i <= i1; ++i, row += 3)
        {
            const float qx = std::abs ((float) i + 0.5f - cx) - (hw - corner);
            const float d = std::hypot (jmax (qx, 0.0f), jmax (qy, 0.0f)) + jmin (jmax (qx, qy), 0.0f) - corner;
            const float core = d <= 0.0f ? 1.0f : std::exp (-d / edge), glowAmt = d <= 0.0f ? 1.0f : std::exp (-d / halo);
            const float v = level * ((1.0f - haloShare) * core + haloShare * glowAmt);
            row[0] += v * p.r; row[1] += v * p.g; row[2] += v * p.b;
        }
    }
}

void Stripe::render (int w, int h, float s)
{
    if (w != bufW || h != bufH || s != glowScale)
    {
        bufW = w; bufH = h;
        renderGlow (w, h, s);
        image = Image (Image::RGB, w, h, false, SoftwareImageType());
        // what is under the stripe (the editor paints it), to add the light to
        background = Image (Image::RGB, w, h, true, SoftwareImageType());
        {
            Graphics bg (background);
            bg.addTransform (AffineTransform::translation (-left, -top).scaled (s));
            if (paintBackground) paintBackground (bg);
        }
        Image::BitmapData src (background, Image::BitmapData::readOnly);
        under.resize ((size_t) (w * h * 3));
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
            {
                const auto c = src.getPixelColour (i, j);
                float* u = under.data() + (size_t) ((j * w + i) * 3);
                u[0] = c.getFloatRed(); u[1] = c.getFloatGreen(); u[2] = c.getFloatBlue();
            }
    }
    light.resize (glow.size());
    for (size_t k = 0; k < glow.size(); ++k) light[k] = glow[k] * envelope;
    for (auto& p : particles)
    {
        const float age = (float) (clock - p.born);
        if (age < 0.0f || age > p.life) continue;
        float a = 1.0f;
        if (p.kind != Kind::flare)
        {
            a = jmin (1.0f, age / 0.015f) * jmin (1.0f, (p.life - age) / (0.3f * p.life));
            a *= 0.8f + 0.2f * std::sin (p.flicker + p.rate * age);
        }
        if (a * envelope > 0.005f) splat (p, p.level * a * envelope, s);
    }
    Image::BitmapData bd (image, Image::BitmapData::writeOnly);
    for (int j = 0; j < h; ++j)
    {
        const float* src = light.data() + (size_t) (j * w) * 3;
        const float* u = under.data() + (size_t) (j * w) * 3;
        for (int i = 0; i < w; ++i, src += 3, u += 3)
        {
            float r = src[0], g = src[1], b = src[2];
            // light past white spills into the other channels (bright cores turn white)
            const float m = jmax (r, g, b);
            if (m > 1.0f) { const float spill = (m - 1.0f) * 0.6f; r += spill; g += spill; b += spill; }
            // added to what is under it
            r = jmin (1.0f, r + u[0]); g = jmin (1.0f, g + u[1]); b = jmin (1.0f, b + u[2]);
            auto* px = reinterpret_cast<PixelRGB*> (bd.getPixelPointer (i, j));
            px->setARGB (255, (uint8) (r * 255.0f + 0.5f), (uint8) (g * 255.0f + 0.5f), (uint8) (b * 255.0f + 0.5f));
        }
    }
}

void Stripe::paint (Graphics& g)
{
    const float s = theme::pixelScale (g, *this);
    const int w = jmax (1, roundToInt (width * s)), h = jmax (1, roundToInt (height * s));
    if (envelope <= 0.0f && background.isValid() && w == bufW && h == bufH && s == glowScale)
        g.drawImage (background, Rectangle<float> (0.0f, 0.0f, width, height));     // (dark: only what is under it)
    else
    {
        render (w, h, s);
        g.drawImage (image, Rectangle<float> (0.0f, 0.0f, width, height));
    }
}
}
