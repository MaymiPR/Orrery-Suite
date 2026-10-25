#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Colours and type measured on the reference at 200 % (RESEARCH.md "GUI"; values in design pixels = 100 %).
namespace ap::theme
{
inline const juce::Colour topBar { 0xff050607 };
inline const juce::Colour panel { 0xff010101 };
inline const juce::Colour text { 0xffe7f1f4 };            // top bar labels, white boxes
inline const juce::Colour boxFill { 0xff121211 };
inline const juce::Colour boxLine { 0xff7f7e7a };         // preset / scale boxes, gear, logo
inline const juce::Colour mint { 0xff8edbc4 };            // readouts, labels, power buttons, peak LED
inline const juce::Colour ledGreen { 0xff00824b };
inline const juce::Colour target { 0xff00824b };          // target brackets
inline const juce::Colour targetLine { 0xff00824b };
inline const juce::Colour targetFill { 0xff00824b };      // a target the level sits in: solid
inline const juce::Colour divider { 0xff3c3c3c };
inline const juce::Colour outline { 0xffc0c0c0 };         // main panel outline
inline const juce::Colour plateText { 0xffbcbabb };       // labels on the bottom plate, bypassed amount text
inline const juce::Colour dimLabel { 0xff808080 };        // GR / limiter labels, readouts, brackets of a bypassed section
inline const juce::Colour clip { 0xffe8513d };            // peak readout over 0 dBFS
inline const juce::Colour menuCaption { 0xffc8785a };     // FACTORY / USER captions in the preset list

// The bundled typeface (Instrument Sans, SIL OFL: the closest free match to the reference's Whyte).
juce::Font font (float emPx);

// draws one line of text at a fractional position: x is the left edge (or the right edge / centre with
// Justification::right / horizontallyCentred), y the baseline (Graphics::drawSingleLineText rounds both to ints)
void textAt (juce::Graphics&, const juce::String&, float x, float baseline,
           juce::Justification j = juce::Justification::left);

// device pixels per design pixel when painting component c: the context's own factor (display scale), or the
// component's transforms up the hierarchy (UI Scale), whichever is larger (snapshots report only the latter)
inline float pixelScale (juce::Graphics& g, const juce::Component& c)
{
    return juce::jmax (g.getInternalContext().getPhysicalPixelScaleFactor(), juce::Component::getApproximateScaleFactorForComponent (&c));
}

// true while the editor is being resized (its "liveResize" property): the per-scale images (glass, plate, shadows,
// knob bodies) are then drawn stretched from the last size instead of rendered again, and re-rendered once it settles
inline bool liveResizing (const juce::Component& c)
{
    for (auto* p = &c; p != nullptr; p = p->getParentComponent())
        if (p->getProperties().getWithDefault ("liveResize", false)) return true;
    return false;
}

// LED meters: LED k (0 at the bottom, 39 at the top) lights at -58.5 + 1.5 k dB (measured with a level ramp).
inline constexpr int numLeds = 40;
inline float ledThresholdDb (int k) { return -58.5f + 1.5f * (float) k; }
}
