#pragma once
#include "Widgets.h"

// "save preset": a name field and save / cancel (layout measured on The God Particle; Orion's is the same).
namespace orrery
{
class SaveDialog : public Dialog
{
public:
    // save: stores the preset under the name, false if that failed; saved: called after a successful save
    SaveDialog (juce::String currentName, std::function<bool (const juce::String&)> saveFn, std::function<void()> saved)
        : Dialog ("save preset", { 240.0f, 74.0f, 408.0f, 209.0f }, true), save (std::move (saveFn)), onSaved (std::move (saved))
    {
        addAndMakeVisible (name);
        name.setBounds (juce::Rectangle<float> (266.0f, 168.0f, 356.0f, 22.0f).toNearestInt());
        name.setFont (theme::font (16.0f));
        name.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        name.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        name.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        name.setColour (juce::TextEditor::textColourId, theme::text);
        name.setColour (juce::TextEditor::highlightColourId, juce::Colour (0xff6b8890));
        name.setColour (juce::CaretComponent::caretColourId, theme::text);
        name.setText (currentName, false);
        name.onReturnKey = [this] { commit(); };
        name.onEscapeKey = [this] { if (onClose) onClose(); };
    }
    // centred on a box placed for an editor of another height (the reference centres it in its own window)
    void moveBy (float dy) { box = box.translated (0.0f, dy); name.setBounds (name.getBounds().translated (0, juce::roundToInt (dy))); saveArea = saveArea.translated (0.0f, dy); cancelArea = cancelArea.translated (0.0f, dy); fieldTop += dy; }
    void focusName() { name.grabKeyboardFocus(); name.selectAll(); }
    void paint (juce::Graphics& g) override
    {
        Dialog::paint (g);
        g.setColour (theme::text.withAlpha (0.9f));
        g.drawRect (juce::Rectangle<float> (258.0f, fieldTop, 371.5f, 47.0f), 1.0f);
        g.setColour (juce::Colour (0xff8c8c8c));
        g.setFont (theme::font (12.5f));
        theme::textAt (g, "preset name", 267.5f, fieldTop + 18.0f);
        button (g, saveArea, "save");
        button (g, cancelArea, "cancel");
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (saveArea.contains (e.position)) { commit(); return; }
        if (cancelArea.contains (e.position)) { if (onClose) onClose(); return; }
        Dialog::mouseDown (e);
    }
private:
    void commit()
    {
        if (save && save (name.getText())) { if (onSaved) onSaved(); }
        if (onClose) onClose();
    }
    static void button (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t)
    {
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::text);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setFont (theme::font (14.5f));
        g.drawText (t, r, juce::Justification::centred, false);
    }
    juce::Rectangle<float> saveArea { 297.0f, 230.5f, 179.0f, 35.5f }, cancelArea { 501.0f, 230.5f, 89.5f, 35.5f };
    float fieldTop = 146.5f;
    std::function<bool (const juce::String&)> save;
    std::function<void()> onSaved;
    juce::TextEditor name;
};
}
