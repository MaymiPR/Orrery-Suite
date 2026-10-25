// Renders the The Antiparticle editor to a PNG for the visual comparison with the reference (tools/ui_compare.py):
//   AntiparticleSnapshot out.png [--scale=2] [paramID=value ...] [--levels=inDb,outDb] [--audio] [--ext] [--anti | --god]
// Parameter values are in real units (dB, 0..1); --levels freezes the I/O meters, --audio shows the particles.
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Settings.h"
#include <iostream>

int main (int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: AntiparticleSnapshot out.png [--scale=2] [paramID=value ...]\n"; return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    auto proc = std::make_unique<AntiparticleProcessor>();
    float scale = 1.0f, inDb = -200.0f, outDb = -200.0f;
    bool audio = false, anti = false, god = false;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a.startsWith ("--scale=")) { scale = a.fromFirstOccurrenceOf ("=", false, false).getFloatValue(); continue; }
        if (a.startsWith ("--levels="))
        {
            inDb = a.fromFirstOccurrenceOf ("=", false, false).upToFirstOccurrenceOf (",", false, false).getFloatValue();
            outDb = a.fromFirstOccurrenceOf (",", false, false).getFloatValue();
            continue;
        }
        if (a == "--audio") { audio = true; continue; }
        if (a == "--ext") { proc->sidechainExternal = true; continue; }
        if (a == "--anti") { anti = true; continue; }
        if (a == "--god") { god = true; continue; }
        for (auto* p : proc->getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (r->getParameterID() == a.upToFirstOccurrenceOf ("=", false, false))
                    r->setValueNotifyingHost (r->convertTo0to1 (a.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
    }
    // the particle style is a global setting: switch it for this render only
    auto& settings = ap::Settings::get();
    const auto oldStyle = settings.particleStyle();
    if (anti) settings.setParticleStyle (ap::Settings::Particles::antiparticle);
    if (god) settings.setParticleStyle (ap::Settings::Particles::godParticle);
    proc->uiScale = scale;      // rendered through the UI Scale path, as in a host
    std::unique_ptr<AntiparticleEditor> ed (dynamic_cast<AntiparticleEditor*> (proc->createEditor()));
    ed->setTestLevels (inDb, outDb, audio);
    if (audio) for (int i = 0; i < 20; ++i) ed->setTestLevels (inDb, outDb, true);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    ed.reset();
    settings.setParticleStyle (oldStyle);
    return 0;
}
