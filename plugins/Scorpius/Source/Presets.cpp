#include "Presets.h"
#include "Params.h"

using namespace juce;

namespace sc
{
namespace
{
// the reference's factory presets, read from its editor and host parameters (RESEARCH.md): each is "start me up" (the
// parameters' defaults) with these values changed. Values are in real units (dB, 0 ... 1, Hz; colour 0 = a, mode 0 = a).
struct Factory { const char* name; std::vector<std::pair<const char*, float>> values; };
const std::vector<Factory>& factory()
{
    static const std::vector<Factory> f {
        { "c mode", { { id::driveColor, 2.0f }, { id::compMode, 0.0f } } },
        { "easy does it", { { id::driveColor, 2.0f }, { id::compMode, 0.0f }, { id::liftAmount, 0.5f } } },
        { "free at last", { { id::compOn, 0.0f } } },
        { "fruity", { { id::inputGain, 9.12f }, { id::compMode, 0.0f }, { id::compAmount, 0.76f }, { id::liftAmount, 0.7f },
                      { id::gritOn, 1.0f }, { id::gritLevel, 0.189f } } },
        { "louder", { { id::harmFreq, 50.0f }, { id::liftAmount, 1.0f }, { id::loudOn, 1.0f }, { id::loudLevel, 0.731f } } },
        { "low end theory", { { id::driveHiPass, 0.0f }, { id::harmFreq, 50.0f }, { id::eqOn, 1.0f }, { id::eq40, 6.0f } } },
        { "parallel me in", { { id::harmAmount, 1.0f }, { id::compAmount, 1.0f }, { id::liftAmount, 1.0f } } },
        { "start me up", {} },
        { "top of the morning", { { id::driveOn, 0.0f }, { id::harmOn, 0.0f }, { id::harmAmount, 1.0f }, { id::compOn, 0.0f },
                                  { id::compAmount, 1.0f }, { id::liftOn, 0.0f }, { id::liftAmount, 1.0f }, { id::gritOn, 1.0f },
                                  { id::gritLevel, 0.731f } } },
    };
    return f;
}

void setReal (AudioProcessorValueTreeState& s, const char* pid, float v)
{
    if (auto* p = dynamic_cast<RangedAudioParameter*> (s.getParameter (pid)))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}
}

File PresetManager::userFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Scorpius").getChildFile ("Presets");
}

std::vector<PresetManager::Entry> PresetManager::list() const
{
    std::vector<Entry> out;
    for (auto& f : factory()) out.push_back ({ f.name, true, {} });
    auto files = userFolder().findChildFiles (File::findFiles, false, "*.scorpius");
    files.sort();
    for (auto& f : files) out.push_back ({ f.getFileNameWithoutExtension(), false, f });
    return out;
}

void PresetManager::load (const Entry& e)
{
    if (e.factory)
    {
        for (auto& f : factory())
            if (e.name == f.name)
            {
                for (auto* pid : presetIds())
                    if (auto* p = apvts.getParameter (pid)) p->setValueNotifyingHost (p->getDefaultValue());
                for (auto& [pid, v] : f.values) setReal (apvts, pid, v);
            }
    }
    else
    {
        auto xml = parseXML (e.file);
        if (xml == nullptr || ! xml->hasTagName ("ScorpiusPreset")) return;
        for (auto* pid : presetIds())
            if (xml->hasAttribute (pid)) setReal (apvts, pid, (float) xml->getDoubleAttribute (pid));
    }
    current = e.name;
}

void PresetManager::step (int delta)
{
    const auto all = list();
    int i = 0;
    for (int k = 0; k < (int) all.size(); ++k)
        if (all[(size_t) k].name == current) i = k;
    i = (i + delta + (int) all.size()) % (int) all.size();
    load (all[(size_t) i]);
}

bool PresetManager::save (const String& name)
{
    if (name.trim().isEmpty()) return false;
    XmlElement xml ("ScorpiusPreset");
    for (auto* pid : presetIds())
        if (auto* p = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (pid)))
            xml.setAttribute (pid, (double) p->convertFrom0to1 (p->getValue()));
    const auto f = fileFor (name.trim());
    if (! f.getParentDirectory().createDirectory() || ! xml.writeTo (f)) return false;
    current = name.trim();
    return true;
}
}
