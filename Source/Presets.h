#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Presets as in the reference: a factory "default" (the reference's defaults) and user presets saved by name.
// A preset holds the EQ, character and limiter settings; input, output and the side-chain switch are left alone.
namespace ap
{
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    struct Entry { juce::String name; bool factory; juce::File file; };

    static juce::File userFolder();
    std::vector<Entry> list() const;                     // factory first, then user presets by name
    juce::String currentName() const { return current; }
    void setCurrentName (const juce::String& n) { current = n; }

    void load (const Entry&);
    void step (int delta);                               // the arrows: previous / next in list(), wrapping
    bool save (const juce::String& name);                // writes userFolder()/name.antp; false on failure
    bool exists (const juce::String& name) const { return fileFor (name).existsAsFile(); }

private:
    juce::File fileFor (const juce::String& name) const { return userFolder().getChildFile (juce::File::createLegalFileName (name) + ".antp"); }
    juce::AudioProcessorValueTreeState& apvts;
    juce::String current { "default" };
};
}
