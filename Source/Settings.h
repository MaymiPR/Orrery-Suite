#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

// The settings page's options. Like the reference they are global (shared by every instance and project), not part
// of the plug-in state: oversampling ratio, I/O meter mode and the particle animation.
namespace dp
{
class Settings : public juce::ChangeBroadcaster
{
public:
    enum class Meter { rmsPeak, lufsPeak };

    static Settings& get()
    {
        static Settings s;
        return s;
    }

    int oversampling() const { return file->getIntValue ("oversampling", 1); }          // 1, 2, 4 or 8
    Meter meterMode() const { return file->getIntValue ("meterMode", 0) == 1 ? Meter::lufsPeak : Meter::rmsPeak; }
    bool animation() const { return file->getBoolValue ("animation", true); }

    void setOversampling (int r) { set ("oversampling", r); }
    void setMeterMode (Meter m) { set ("meterMode", m == Meter::lufsPeak ? 1 : 0); }
    void setAnimation (bool on) { set ("animation", on); }

private:
    Settings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Devil's Particle";
        o.filenameSuffix = ".settings";
        o.folderName = "Devil's Particle";
        o.osxLibrarySubFolder = "Application Support";
        file = std::make_unique<juce::PropertiesFile> (o);
    }
    void set (const juce::String& key, const juce::var& v)
    {
        file->setValue (key, v);
        file->saveIfNeeded();
        sendChangeMessage();
    }
    std::unique_ptr<juce::PropertiesFile> file;
};
}
