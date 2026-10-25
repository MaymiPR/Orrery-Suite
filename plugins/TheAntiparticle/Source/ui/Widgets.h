#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "Theme.h"

// Small controls of the editor, drawn like the reference (all positions in design pixels; the editor scales them).
namespace ap
{
// never take keyboard focus: the host keeps its shortcuts (space bar in FL Studio)
inline void noFocus (juce::Component& c)
{
    c.setWantsKeyboardFocus (false);
    c.setMouseClickGrabsKeyboardFocus (false);
}

// Mint power button: filled while the section runs, outlined while it is bypassed.
class PowerButton : public juce::Component
{
public:
    PowerButton() { noFocus (*this); setRepaintsOnMouseActivity (true); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (contains (e.getPosition()) && onClick) onClick(); }
    bool active = true;
    std::function<void()> onClick;
};

// I/O LINK toggle of the top bar, beside the side-chain switch (ours, not on the reference): drawn like a segment of
// INT / EXT, filled while linked (the output gain then mirrors the input gain).
class LinkButton : public juce::Component
{
public:
    LinkButton() { noFocus (*this); setRepaintsOnMouseActivity (true); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (contains (e.getPosition()) && onClick) onClick(); }
    bool active = false;
    std::function<void()> onClick;
};

// Boxed icon button of the top bar: previous / next preset, save.
class IconButton : public juce::Component
{
public:
    enum class Icon { left, right, save };
    explicit IconButton (Icon i) : icon (i) { noFocus (*this); setRepaintsOnMouseActivity (true); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
private:
    Icon icon;
};

// Drop-down box (preset name, UI scale): grey frame, text, white triangle.
class DropBox : public juce::Component
{
public:
    DropBox() { noFocus (*this); setRepaintsOnMouseActivity (true); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    juce::String text;
    bool open = false;
    std::function<void()> onClick;
};

// INT | EXT segmented switch (side-chain source).
class SegmentToggle : public juce::Component
{
public:
    SegmentToggle() { noFocus (*this); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override
    {
        const bool ext = e.x > getWidth() / 2;
        if (ext != external) { external = ext; repaint(); if (onChange) onChange (external); }
    }
    bool external = false;
    std::function<void (bool)> onChange;
};

// Gear icon (settings).
class GearButton : public juce::Component
{
public:
    GearButton() { noFocus (*this); setRepaintsOnMouseActivity (true); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
};

// The Orrery Suite mark (left of the top bar), drawn as the reference draws its maker's mark there: a flat grey glyph
// in the same place, size and grey.
class Logo : public juce::Component
{
public:
    Logo() { noFocus (*this); setRepaintsOnMouseActivity (true); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    std::function<void()> onClick;
};


// List that drops below a DropBox (preset list with captions, UI scale list). Closes on a pick or a click outside.
class DropList : public juce::Component
{
public:
    struct Item { juce::String text; bool caption = false; bool selected = false; };
    DropList() { noFocus (*this); }
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent& e) override { hover = rowAt (e.y); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override;
    void setItems (std::vector<Item> i, float rowHeight, bool captionsStyle);
    std::vector<Item> items;
    std::function<void (int)> onPick;   // index into items
    float rowH = 24.0f;
    bool presetStyle = false;
private:
    int rowAt (int y) const;
    int hover = -1;
};

// Modal panel over the editor (settings, save preset): dims what is below and draws a titled box.
class Dialog : public juce::Component
{
public:
    Dialog (juce::String title, juce::Rectangle<float> box, bool dimTopBar);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    std::function<void()> onClose;
    juce::Rectangle<float> box;
protected:
    juce::String title;
    bool dimTop;
    juce::Rectangle<float> closeArea() const { return { box.getRight() - 33.0f, box.getY() + 4.0f, 26.0f, 22.0f }; }
};

// "About" panel: label / value rows
class InfoDialog : public Dialog
{
public:
    InfoDialog (juce::String title, std::vector<std::pair<juce::String, juce::String>> rows);
    void paint (juce::Graphics&) override;
private:
    std::vector<std::pair<juce::String, juce::String>> rows;
};
}
