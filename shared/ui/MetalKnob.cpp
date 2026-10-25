#include "MetalKnob.h"
#include "Theme.h"

using namespace juce;

namespace orrery
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

float MetalKnob::designY (const MouseEvent& e) const
{
    return e.position.y * std::abs (getTransform().mat00);    // the knob's own placement transform undone
}

void MetalKnob::mouseDown (const MouseEvent& e)
{
    Slider::mouseDown (e);                       // begins the host gesture; rotary drag modes do not jump on click
    lastY = anchorY = designY (e);
    anchorValue = valueToProportionOfLength (getValue());
    downScreen = e.getScreenPosition().toFloat();
    velocity = false;
    if (e.mods.isAltDown()) setValue (defaultValue, sendNotificationSync);
}

void MetalKnob::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isAltDown()) return;
    const float y = designY (e);
    if (velocityKeys (e.mods))
    {
        const float diff = y - lastY;
        const double speed = jlimit (0.0, 200.0, (double) std::abs (diff));
        if (speed > 0.0)
        {
            double step = 0.2 * (1.0 + std::sin (MathConstants<double>::pi * (1.5 + jmin (0.5, jmax (0.0, speed - 1.0) / 200.0))));
            if (diff > 0.0f) step = -step;                  // up raises the value
            setValue (proportionOfLengthToValue (jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + step)), sendNotificationSync);
            e.source.enableUnboundedMouseMovement (true, false);
            velocity = true;
        }
    }
    else
    {
        if (velocity) { restoreMouse(); lastY = anchorY; return; }
        setValue (proportionOfLengthToValue (jlimit (0.0, 1.0, anchorValue + (anchorY - y) * 0.01)), sendNotificationSync);
    }
    lastY = y;
}

void MetalKnob::mouseUp (const MouseEvent& e)
{
    if (velocity) restoreMouse();
    Slider::mouseUp (e);
}

void MetalKnob::modifierKeysChanged (const ModifierKeys& m)
{
    if (velocity && isMouseButtonDown() && ! velocityKeys (m)) restoreMouse();
    Slider::modifierKeysChanged (m);
}

// as JUCE's slider: the cursor reappears where an absolute drag from the press point would have put it for the
// current value, kept inside the knob, and the drag continues absolutely from there
void MetalKnob::restoreMouse()
{
    velocity = false;
    auto src = Desktop::getInstance().getMainMouseSource();
    src.enableUnboundedMouseMovement (false);
    const double now = valueToProportionOfLength (getValue());
    const float screenPerDesign = getScreenBounds().toFloat().getHeight() / ((float) getHeight() * std::abs (getTransform().mat00));
    auto pos = downScreen.translated (0.0f, (float) ((anchorValue - now) * 100.0) * screenPerDesign);
    pos = getScreenBounds().reduced (4).toFloat().getConstrainedPoint (pos);
    src.setScreenPosition (pos);
    anchorY = getLocalPoint (nullptr, pos).y * std::abs (getTransform().mat00);
    anchorValue = now;
    downScreen = pos;
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
                    for (int k = 1; k <= 16; ++k)
                        g += model.coef[2 * k - 1] * std::cos ((float) k * a) + model.coef[2 * k] * std::sin ((float) k * a);
                    float v;
                    if (r < 0.88f)
                        v = g * interp (model.radial, 12, 0.05f, 0.90f, r);
                    else
                    {
                        // rim bands at r = 0.885 ... 0.995 (0.005 apart), linear between bands (and from the body at 0.88)
                        auto band = [&] (int i)
                        {
                            const float* c = model.rimCoef[i];
                            float s = c[0];
                            for (int k = 1; k <= 8; ++k) s += c[2 * k - 1] * std::cos ((float) k * a) + c[2 * k] * std::sin ((float) k * a);
                            return s;
                        };
                        const float t = (r - 0.885f) / 0.005f;
                        if (t <= 0.0f)
                        {
                            const float body0 = g * interp (model.radial, 12, 0.05f, 0.90f, 0.88f);
                            v = body0 + (band (0) - body0) * jlimit (0.0f, 1.0f, (r - 0.88f) / 0.005f);
                        }
                        else
                        {
                            const int i = jmin (21, (int) t);
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
    const float scale = theme::pixelScale (g, *this);
    const int px = jmax (4, roundToInt (b.getWidth() * scale));
    if (body.isNull() || (px != bodyPx && ! theme::liveResizing (*this))) renderBody (px);

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
