// Renders the Devil's Particle editor to a PNG for the visual comparison with the reference (tools/ui_compare.py):
//   DevilsParticleSnapshot out.png [--scale=2] [paramID=value ...] [--levels=inDb,outDb] [--audio] [--ext]
// Parameter values are in real units (dB, 0..1); --levels freezes the I/O meters, --audio shows the particles.
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

int main (int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: DevilsParticleSnapshot out.png [--scale=2] [paramID=value ...]\n"; return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    auto proc = std::make_unique<DevilsParticleProcessor>();
    float scale = 1.0f, inDb = -200.0f, outDb = -200.0f;
    bool audio = false;
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
        for (auto* p : proc->getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (r->getParameterID() == a.upToFirstOccurrenceOf ("=", false, false))
                    r->setValueNotifyingHost (r->convertTo0to1 (a.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
    }
    proc->uiScale = scale;      // rendered through the UI Scale path, as in a host
    std::unique_ptr<DevilsParticleEditor> ed (dynamic_cast<DevilsParticleEditor*> (proc->createEditor()));
    ed->setTestLevels (inDb, outDb, audio);
    if (audio) for (int i = 0; i < 20; ++i) ed->setTestLevels (inDb, outDb, true);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    ed.reset();
    return 0;
}
