#include "Sky.h"
#include <cmath>

using namespace juce;

namespace sc
{
namespace
{
// Scorpius's main stars: right ascension and declination (degrees), magnitude and colour (B stars blue-white, the
// red supergiant Antares, the orange giants epsilon and zeta2, the yellow-white theta)
struct Catalogue { const char* name; float ra, dec, mag, r, g, b; };
constexpr Catalogue catalogue[] {
    { "beta",    241.36f, -19.81f, 2.6f, 0.78f, 0.86f, 1.00f },
    { "delta",   240.08f, -22.62f, 2.3f, 0.78f, 0.86f, 1.00f },
    { "pi",      239.71f, -26.11f, 2.9f, 0.78f, 0.86f, 1.00f },
    { "rho",     239.22f, -29.21f, 3.9f, 0.78f, 0.86f, 1.00f },
    { "sigma",   245.30f, -25.59f, 2.9f, 0.78f, 0.86f, 1.00f },
    { "antares", 247.35f, -26.43f, 1.0f, 1.00f, 0.32f, 0.16f },
    { "tau",     248.97f, -28.22f, 2.8f, 0.78f, 0.86f, 1.00f },
    { "epsilon", 252.54f, -34.29f, 2.3f, 1.00f, 0.80f, 0.55f },
    { "mu1",     252.97f, -38.05f, 3.0f, 0.78f, 0.86f, 1.00f },
    { "zeta2",   253.65f, -42.36f, 3.6f, 1.00f, 0.80f, 0.55f },
    { "eta",     258.04f, -43.24f, 3.3f, 0.95f, 0.95f, 1.00f },
    { "theta",   264.33f, -43.00f, 1.9f, 1.00f, 0.97f, 0.88f },
    { "iota1",   266.90f, -40.13f, 3.0f, 1.00f, 0.97f, 0.88f },
    { "kappa",   265.62f, -39.03f, 2.4f, 0.78f, 0.86f, 1.00f },
    { "lambda",  263.40f, -37.10f, 1.6f, 0.78f, 0.86f, 1.00f },
    { "upsilon", 262.69f, -37.30f, 2.7f, 0.78f, 0.86f, 1.00f } };
constexpr int antares = 5;
// the lines of the figure, and the body from the head (delta) to the stinger (lambda, upsilon)
constexpr int lines[][2] { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 1, 4 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 8 }, { 8, 9 },
                           { 9, 10 }, { 10, 11 }, { 11, 12 }, { 12, 13 }, { 13, 14 }, { 14, 15 } };
constexpr int bodyStars[] { 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

// the sky into the stripe (design px from its corner): east to the left, as seen
Point<float> place (float ra, float dec)
{
    return { 800.0f - (ra - 239.2f) / 27.7f * 730.0f, 3.5f + (-19.8f - dec) / 23.5f * 29.0f };
}

float noise2 (int x, int y)
{
    uint32 h = (uint32) x * 374761393u + (uint32) y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float) ((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}

float smoothNoise (float x, float y)
{
    const int xi = (int) std::floor (x), yi = (int) std::floor (y);
    const float fx = x - (float) xi, fy = y - (float) yi;
    const float a = noise2 (xi, yi), b = noise2 (xi + 1, yi), c = noise2 (xi, yi + 1), d = noise2 (xi + 1, yi + 1);
    const float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}
}

Sky::Sky()
{
    for (auto& c : catalogue)
    {
        const auto p = place (c.ra, c.dec);
        stars.push_back ({ p.x, p.y, std::pow (10.0f, -0.4f * (c.mag - 1.0f)), c.r, c.g, c.b,
                           MathConstants<float>::twoPi * rng.nextFloat(), 1.5f + 3.0f * rng.nextFloat() });
    }
    for (int i : bodyStars) body.push_back ({ stars[(size_t) i].x, stars[(size_t) i].y });
    bodyLength.push_back (0.0f);
    for (size_t i = 1; i < body.size(); ++i) bodyLength.push_back (bodyLength.back() + body[i].getDistanceFrom (body[i - 1]));
    // dust stars, thicker towards the tail (where the Milky Way runs)
    for (int i = 0; i < 560; ++i)
    {
        const float x = 868.0f * std::pow (rng.nextFloat(), 1.4f);
        const float kind = rng.nextFloat();                 // most white, some blue, some warm
        const float r = kind < 0.2f ? 0.7f : 1.0f, g = kind < 0.2f ? 0.8f : kind > 0.8f ? 0.75f : 0.95f, b = kind > 0.8f ? 0.55f : 1.0f;
        dust.push_back ({ x, 1.5f + 33.0f * rng.nextFloat(), 0.2f + 0.8f * std::pow (rng.nextFloat(), 3.0f), r, g, b,
                          MathConstants<float>::twoPi * rng.nextFloat(), 3.0f + 9.0f * rng.nextFloat() });
    }
}

Point<float> Sky::along (float d) const
{
    for (size_t i = 1; i < body.size(); ++i)
        if (d <= bodyLength[i])
        {
            const float u = (d - bodyLength[i - 1]) / jmax (0.001f, bodyLength[i] - bodyLength[i - 1]);
            return body[i - 1] + (body[i] - body[i - 1]) * u;
        }
    return body.back();
}

void Sky::advance (float dt, float brightness, bool beat)
{
    time += dt;
    heart = beat ? 1.0f : heart * std::exp (-dt / 0.3f);
    // venom: shoots from the head along the body like meteors; a few all the time, more on a beat
    spawnDebt += dt * 5.0f * brightness + (beat ? 4.0f : 0.0f);
    while (spawnDebt >= 1.0f && sparks.size() < 400)
    {
        spawnDebt -= 1.0f;
        sparks.push_back ({ 0, 0, 260.0f + 260.0f * rng.nextFloat(), (rng.nextFloat() - 0.5f) * 2.4f, 0.0f, 99.0f,
                            -60.0f * rng.nextFloat(), true });
    }
    spawnDebt = jmin (spawnDebt, 1.0f);
    const float total = bodyLength.back();
    std::vector<Spark> born;
    for (auto& s : sparks)
    {
        s.age += dt;
        if (s.venom)
        {
            s.along += s.vx * dt;
            if (s.along >= total)
            {
                // at the stinger: a few sparks fly off
                s.life = -1.0f;
                for (int k = 0; k < 6; ++k)
                {
                    const float a = MathConstants<float>::twoPi * rng.nextFloat(), v = 25.0f + 70.0f * rng.nextFloat();
                    born.push_back ({ body.back().x, body.back().y, v * std::cos (a), 0.5f * v * std::sin (a), 0.0f,
                                      0.25f + 0.3f * rng.nextFloat(), 0.0f, false });
                }
            }
        }
        else
        {
            s.x += s.vx * dt;
            s.y += s.vy * dt;
        }
    }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const Spark& s) { return s.age > s.life || s.life < 0.0f; }), sparks.end());
    for (auto& b : born) sparks.push_back (b);
}

void Sky::addDot (std::vector<float>& light, int w, int h, float s, float x, float y, float radius, float halo, float haloShare,
                  float r, float g, float b, float level) const
{
    if (level <= 0.002f) return;
    const float cx = x * s, cy = y * s, rc = jmax (0.35f, radius * s), rh = halo * s;
    const float reach = rc + 3.0f * rh;
    const int i0 = jmax (0, (int) (cx - reach)), i1 = jmin (w - 1, (int) (cx + reach) + 1);
    const int j0 = jmax (0, (int) (cy - reach)), j1 = jmin (h - 1, (int) (cy + reach) + 1);
    for (int j = j0; j <= j1; ++j)
        for (int i = i0; i <= i1; ++i)
        {
            const float d = std::hypot ((float) i + 0.5f - cx, (float) j + 0.5f - cy);
            const float core = d <= rc ? 1.0f : std::exp (-(d - rc) / 0.45f);
            const float glow = std::exp (-d / jmax (0.3f, rh));
            const float v = level * ((1.0f - haloShare) * core + haloShare * glow);
            float* px = light.data() + (size_t) ((j * w + i) * 3);
            px[0] += v * r; px[1] += v * g; px[2] += v * b;
        }
}

void Sky::addLine (std::vector<float>& light, int w, int h, float s, Point<float> a, Point<float> b, float level,
                   float r, float g, float bl) const
{
    const float len = a.getDistanceFrom (b) * s;
    const int steps = jmax (1, (int) (len / 0.7f));
    for (int k = 0; k <= steps; ++k)
    {
        const auto p = a + (b - a) * ((float) k / (float) steps);
        const int i = (int) (p.x * s), j = (int) (p.y * s);
        if (i < 0 || j < 0 || i >= w || j >= h) continue;
        float* px = light.data() + (size_t) ((j * w + i) * 3);
        px[0] += level * r; px[1] += level * g; px[2] += level * bl;
    }
}

void Sky::render (std::vector<float>& light, int w, int h, float s, float brightness)
{
    // the Milky Way: a warm band of dust across the tail, rendered once per size
    if (w != hazeW || h != hazeH || s != hazeS)
    {
        hazeW = w; hazeH = h; hazeS = s;
        haze.assign ((size_t) (w * h * 3), 0.0f);
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
            {
                const float x = ((float) i + 0.5f) / s, y = ((float) j + 0.5f) / s;
                const float centre = 26.0f - 16.0f * (x / 420.0f);                 // rising to the right
                const float band = std::exp (-std::pow ((y - centre) / 10.0f, 2.0f)) * jlimit (0.0f, 1.0f, (480.0f - x) / 300.0f);
                const float n = 0.55f + 0.45f * smoothNoise (x / 9.0f, y / 4.0f) + 0.25f * smoothNoise (x / 3.0f, y / 1.5f);
                const float v = 0.085f * band * n;
                float* px = haze.data() + (size_t) ((j * w + i) * 3);
                px[0] = v; px[1] = v * 0.78f; px[2] = v * 0.6f;
            }
    }
    for (size_t k = 0; k < haze.size(); ++k) light[k] += haze[k] * brightness;

    for (auto& d : dust)
        addDot (light, w, h, s, d.x, d.y, 0.25f, 0.6f, 0.25f, d.r, d.g, d.b,
                brightness * d.size * (0.55f + 0.45f * std::sin (d.phase + d.rate * time)));

    // the figure's lines, brighter on a beat
    for (auto& l : lines)
        addLine (light, w, h, s, { stars[(size_t) l[0]].x, stars[(size_t) l[0]].y }, { stars[(size_t) l[1]].x, stars[(size_t) l[1]].y },
                 brightness * (0.1f + 0.12f * heart), 0.75f, 0.8f, 1.0f);

    for (size_t i = 0; i < stars.size(); ++i)
    {
        const auto& st = stars[i];
        const bool heartStar = (int) i == antares;
        const float twinkle = 0.75f + 0.25f * std::sin (st.phase + st.rate * time);
        const float level = brightness * twinkle * (0.9f + 1.1f * st.size) * (heartStar ? 1.0f + 0.8f * heart : 1.0f);
        const float radius = 0.5f + 0.8f * st.size + (heartStar ? 0.8f * heart : 0.0f);
        addDot (light, w, h, s, st.x, st.y, radius, heartStar ? 3.0f + 3.0f * heart : 1.6f, heartStar ? 0.5f : 0.35f, st.r, st.g, st.b, level);
        if (st.size > 0.3f)        // the brighter ones: four faint spikes
        {
            const float len = 2.0f + 5.0f * st.size;
            addLine (light, w, h, s, { st.x - len, st.y }, { st.x + len, st.y }, level * 0.18f, st.r, st.g, st.b);
            addLine (light, w, h, s, { st.x, st.y - len * 0.6f }, { st.x, st.y + len * 0.6f }, level * 0.18f, st.r, st.g, st.b);
        }
    }

    const float total = bodyLength.back();
    for (auto& sp : sparks)
    {
        if (sp.venom)
        {
            if (sp.along < 0.0f) continue;
            const float u = sp.along / total;
            const float r = 1.0f, g = 0.65f - 0.4f * u, b = 0.25f - 0.12f * u;          // amber, reddening towards the sting
            addDot (light, w, h, s, along (sp.along).x, along (sp.along).y + sp.vy, 0.55f, 1.4f, 0.35f, r, g, b, brightness * 1.6f);
            for (int k = 1; k < 64; ++k)                                                 // a bright head, a long trail
            {
                auto p = along (sp.along - (float) k * 0.5f);
                p.y += sp.vy;
                addLine (light, w, h, s, p, p, brightness * 0.5f * std::pow (1.0f - (float) k / 64.0f, 1.5f), r, g, b);
            }
        }
        else
        {
            const float fade = 1.0f - sp.age / sp.life;
            addDot (light, w, h, s, sp.x, sp.y, 0.35f, 1.0f, 0.35f, 1.0f, 0.45f, 0.2f, brightness * fade);
        }
    }
}
}
