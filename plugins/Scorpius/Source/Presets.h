#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Factory presets (RESEARCH.md "Factory presets": the reference's nine, measured) and user presets (.scorpius files
// in Documents/Scorpius/Presets). A preset holds every parameter but the host bypass, the gains included (as the
// reference: "fruity" sets the input to +9.12 dB).
namespace sc
{
class PresetManager
{
public:
    struct Entry { juce::String name; bool factory = false; juce::File file; };

    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    std::vector<Entry> list() const;
    void load (const Entry&);
    void step (int delta);
    bool save (const juce::String& name);
    const juce::String& currentName() const { return current; }
    void setCurrentName (const juce::String& n) { current = n; }

    static juce::File userFolder();
    static juce::String defaultName() { return "start me up"; }

private:
    juce::File fileFor (const juce::String& name) const { return userFolder().getChildFile (juce::File::createLegalFileName (name) + ".scorpius"); }
    juce::AudioProcessorValueTreeState& apvts;
    juce::String current = defaultName();
};
}
