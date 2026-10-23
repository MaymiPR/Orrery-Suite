#include "Presets.h"
#include "Params.h"

using namespace juce;

namespace dp
{
static const char* const presetParams[] { id::eqLow, id::eqMid, id::eqHigh, id::eqBypass, id::character, id::characterBypass,
                                          id::limiterGain, id::limiterBypass };

File PresetManager::userFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Devil's Particle").getChildFile ("Presets");
}

std::vector<PresetManager::Entry> PresetManager::list() const
{
    std::vector<Entry> out { { "default", true, {} } };
    auto files = userFolder().findChildFiles (File::findFiles, false, "*.dpp");
    files.sort();
    for (auto& f : files) out.push_back ({ f.getFileNameWithoutExtension(), false, f });
    return out;
}

void PresetManager::load (const Entry& e)
{
    if (e.factory)
    {
        for (auto* pid : presetParams)
            if (auto* p = apvts.getParameter (pid)) p->setValueNotifyingHost (p->getDefaultValue());
    }
    else
    {
        auto xml = parseXML (e.file);
        if (xml == nullptr || ! xml->hasTagName ("DevilsParticlePreset")) return;
        for (auto* pid : presetParams)
            if (auto* p = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (pid)))
                if (xml->hasAttribute (pid))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) xml->getDoubleAttribute (pid)));
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
    XmlElement xml ("DevilsParticlePreset");
    for (auto* pid : presetParams)
        if (auto* p = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (pid)))
            xml.setAttribute (pid, (double) p->convertFrom0to1 (p->getValue()));
    const auto f = fileFor (name.trim());
    if (! f.getParentDirectory().createDirectory() || ! xml.writeTo (f)) return false;
    current = name.trim();
    return true;
}
}
