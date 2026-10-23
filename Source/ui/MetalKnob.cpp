#include "MetalKnob.h"

using namespace juce;

namespace dp
{
static const knobmodel::Model& modelFor (MetalKnob::Kind k)
{
    return k == MetalKnob::Kind::small ? knobmodel::small : k == MetalKnob::Kind::large ? knobmodel::large : knobmodel::plate;
}

MetalKnob::MetalKnob (Kind k) : model (modelFor (k)), kind (k)
{
    setSliderStyle (RotaryVerticalDrag);
    setTextBoxStyle (NoTextBox, false, 0, 0);
    setRotaryParameters (startAngle + MathConstants<float>::twoPi, endAngle + MathConstants<float>::twoPi, true);
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    setPopupMenuEnabled (false);
}

void MetalKnob::mouseDown (const MouseEvent& e)
{
    Slider::mouseDown (e);                       // begins the host gesture; rotary drag modes do not jump on click
    lastY = e.position.y;
    if (e.mods.isAltDown()) setValue (defaultValue, sendNotificationSync);
}

void MetalKnob::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isAltDown()) return;
    // incremental, so pressing / releasing Shift mid-drag does not jump; distances in the editor's design pixels
    // (the knob's own placement transform is undone)
    const float unit = std::abs (getTransform().mat00);    // uniform scale, no rotation
    const float dy = (e.position.y - lastY) * unit;
    lastY = e.position.y;
    const double perPixel = e.mods.isShiftDown() ? 0.01 / 30.0 : 0.01;
    const double p = jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) - dy * perPixel);
    setValue (proportionOfLengthToValue (p), sendNotificationSync);
}

static float interp (const float* table, int n, float x0, float x1, float x)
{
    const float t = jlimit (0.0f, (float) (n - 1), (x - x0) / (x1 - x0) * (float) (n - 1));
    const int i = jmin (n - 2, (int) t);
    return table[i] + (table[i + 1] - table[i]) * (t - (float) i);
}

void MetalKnob::renderBody (int px)
{
    // px = knob diameter in physical pixels; the image holds the knob only (alpha outside)
    body = Image (Image::ARGB, px, px, true);
    Image::BitmapData bd (body, Image::BitmapData::writeOnly);
    const float R = (float) px * 0.5f;
    for (int y = 0; y < px; ++y)
        for (int x = 0; x < px; ++x)
        {
            float acc[3] {}, cover = 0.0f;
            for (int sy = 0; sy < 3; ++sy)                // 3 x 3 supersampling for a clean edge
                for (int sx = 0; sx < 3; ++sx)
                {
                    const float fx = (float) x + ((float) sx + 0.5f) / 3.0f - R, fy = (float) y + ((float) sy + 0.5f) / 3.0f - R;
                    const float r = std::sqrt (fx * fx + fy * fy) / R;
                    if (r >= 1.0f) continue;
                    const float a = std::atan2 (fx, -fy);    // 0 = up, clockwise
                    float g = model.coef[0];
                    for (int k = 1; k <= 8; ++k)
                        g += model.coef[2 * k - 1] * std::cos ((float) k * a) + model.coef[2 * k] * std::sin ((float) k * a);
                    float v;
                    if (r < 0.88f)
                        v = g * interp (model.radial, 12, 0.05f, 0.90f, r);
                    else
                    {
                        // rim bands at r = 0.89 ... 0.99, linear between bands (and from the body at 0.88)
                        auto band = [&] (int i)
                        {
                            const float* c = model.rimCoef[i];
                            float s = c[0];
                            for (int k = 1; k <= 6; ++k) s += c[2 * k - 1] * std::cos ((float) k * a) + c[2 * k] * std::sin ((float) k * a);
                            return s;
                        };
                        const float t = (r - 0.89f) / 0.02f;
                        if (t <= 0.0f)
                        {
                            const float body0 = g * interp (model.radial, 12, 0.05f, 0.90f, 0.88f);
                            v = body0 + (band (0) - body0) * jlimit (0.0f, 1.0f, (r - 0.88f) / 0.01f);
                        }
                        else
                        {
                            const int i = jmin (4, (int) t);
                            v = band (i) + (band (i + 1) - band (i)) * jmin (1.0f, t - (float) i);
                        }
                    }
                    for (int c = 0; c < 3; ++c) acc[c] += v * model.tint[c];
                    cover += 1.0f;
                }
            if (cover <= 0.0f) continue;
            const float alpha = cover / 9.0f;
            auto ch = [&] (int c) { return (uint8) jlimit (0.0f, 255.0f, acc[c] / cover); };
            bd.setPixelColour (x, y, Colour (ch (0), ch (1), ch (2)).withAlpha (alpha));
        }
    bodyPx = px;
}

void MetalKnob::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int px = jmax (4, roundToInt (b.getWidth() * scale));
    if (px != bodyPx) renderBody (px);

    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (body, b, RectanglePlacement::stretchToFit);

    const float angle = startAngle + (float) valueToProportionOfLength (getValue()) * (endAngle - startAngle);
    const float R = b.getWidth() * 0.5f;
    const auto c = b.getCentre();
    const float w = model.pip[2] * b.getWidth() / (2.0f * R);
    Line<float> l (c.getPointOnCircumference (R * model.pip[0], angle), c.getPointOnCircumference (R * model.pip[1], angle));
    g.setColour (Colour::fromFloatRGBA (model.pipColour[0] / 255.0f, model.pipColour[1] / 255.0f, model.pipColour[2] / 255.0f, 1.0f));
    Path ptr;
    ptr.startNewSubPath (l.getStart());
    ptr.lineTo (l.getEnd());
    g.strokePath (ptr, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
}
}
