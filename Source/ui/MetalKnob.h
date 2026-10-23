#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KnobModel.h"

namespace ap
{
// Brushed-metal knob drawn from the measured lighting model (KnobModel.h): the light stays put, only the pointer
// turns (-135 .. +135 degrees, as the reference). The slider fills the knob's bounding square.
// Mouse, as measured on the reference: vertical drag, 1 % of the range per design pixel (100 px for the whole range
// at any UI scale), Shift = fine (1/30), double-click or Alt-click = default value.
class MetalKnob : public juce::Slider
{
public:
    enum class Kind { small, large, plate };
    explicit MetalKnob (Kind);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void setDefaultValue (double v) { defaultValue = v; setDoubleClickReturnValue (true, v); }

    static constexpr float startAngle = -2.35619449f, endAngle = 2.35619449f;

private:
    const knobmodel::Model& model;
    Kind kind;
    juce::Image body;            // cached at the current physical pixel size
    int bodyPx = 0;
    double defaultValue = 0.0;
    float lastY = 0.0f;
    void renderBody (int px);
};
}
