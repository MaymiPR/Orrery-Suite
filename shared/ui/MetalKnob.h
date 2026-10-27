#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobModel.h"

namespace orrery
{
// Brushed-metal knob drawn from the measured lighting model (KnobModel.h): the light stays put, only the pointer
// turns (-135 .. +135 degrees, as the reference). The slider fills the knob's bounding square.
// Mouse, as measured on the reference (RefCapture drag scripts; it is JUCE's own slider behaviour, in design pixels):
//   - vertical drag, absolute from the press point: 1 % of the range per design pixel (100 px for the whole range at
//     any UI scale), so a drag past an end has to come back that far before the value moves again;
//   - Shift: velocity mode, the cursor hidden. Each mouse move of d px changes the value by
//     0.2 (1 + sin (pi (1.5 + min (0.5, max (0, d - 1) / 200)))) of the range: slow moves fine-tune, fast ones jump.
//     Releasing the key restores the cursor where the value puts it and continues from there. Ctrl (Cmd) drags at the
//     normal speed (as both references);
//   - Alt-click = default value; double-click opens a field to type the value in (Enter or a click elsewhere sets it,
//     Escape leaves it), parsed by the parameter's own text conversion.
class MetalKnob : public juce::Slider
{
public:
    enum class Kind { small, large, plate };
    explicit MetalKnob (Kind);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void modifierKeysChanged (const juce::ModifierKeys&) override;
    void setDefaultValue (double v) { defaultValue = v; }
    // a typed value is set as one host gesture: begin (true) / end (false), when the owner provides them
    std::function<void (bool)> onGesture;
    void showValueEntry();

    static constexpr float startAngle = -2.35619449f, endAngle = 2.35619449f;

private:
    const knobmodel::Model& model;
    Kind kind;
    juce::Image body;            // cached at the current physical pixel size
    int bodyPx = 0;
    double defaultValue = 0.0;
    float lastY = 0.0f, anchorY = 0.0f;           // design pixels
    double anchorValue = 0.0;                      // proportion of the range at the anchor
    bool velocity = false;
    juce::Point<float> downScreen;                 // where the press was (screen)
    float designY (const juce::MouseEvent&) const;
    static bool velocityKeys (const juce::ModifierKeys& m) { return m.isShiftDown(); }
    void restoreMouse();
    void renderBody (int px);
    void closeValueEntry (bool apply);
    std::unique_ptr<juce::TextEditor> entry;       // on the top-level component, over the knob
};
}
