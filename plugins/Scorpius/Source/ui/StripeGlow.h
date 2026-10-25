#pragma once
// The glow behind the stripe's particles, fitted to the reference while it plays (plugins/Scorpius/tools/fit_stripe.py:
// the 20th percentile of its frames at full brightness, less its idle frame). Separable: a profile down the stripe
// (rows 0.5 design px apart) times, per colour channel, a bell along it: a exp (-((x - c) / s)^2) + b exp (-|x - c| / t),
// in grey levels (0 ... 255) at full brightness.
namespace sc::stripeglow
{
constexpr float rowsFrom = 272.25f, rowStep = 0.5f;
constexpr int numRows = 71;
constexpr float rows[numRows] {
    0.295f, 0.072f, 0.000f, 0.124f, 0.226f, 0.213f, 0.222f, 0.226f, 0.231f, 0.236f, 0.242f, 0.247f,
    0.253f, 0.259f, 0.266f, 0.273f, 0.280f, 0.287f, 0.295f, 0.305f, 0.350f, 0.401f, 0.403f, 0.264f,
    0.339f, 0.370f, 0.379f, 0.393f, 0.413f, 0.440f, 0.468f, 0.504f, 0.517f, 0.711f, 1.000f, 0.920f,
    0.817f, 0.703f, 0.736f, 0.827f, 0.685f, 0.494f, 0.478f, 0.627f, 0.815f, 0.669f, 0.442f, 0.405f,
    0.372f, 0.339f, 0.350f, 0.405f, 0.423f, 0.432f, 0.427f, 0.410f, 0.397f, 0.383f, 0.371f, 0.361f,
    0.343f, 0.424f, 0.259f, 0.089f, 0.178f, 0.177f, 0.124f, 0.304f, 0.766f, 0.433f, 0.000f,
};
struct Bell { float c, a, s, b, t; };
constexpr Bell along[3] {   // red, green, blue
    { 449.533f, 115.208f, 227.359f, 69.570f, 37.398f },
    { 452.692f, 101.726f, 225.971f, 58.724f, 22.202f },
    { 454.233f, 127.050f, 229.610f, 50.197f, 19.862f },
};
}
