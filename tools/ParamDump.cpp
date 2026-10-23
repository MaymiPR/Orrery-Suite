// ParamDump: loads a VST3 and prints everything a host can see without audio:
// buses, latency, tail, editor size, and per parameter its name, label, steps, default
// and the text the plug-in shows across the normalised range.
//   ParamDump plugin.vst3 [name] [points=N] [fs=48000]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>

using namespace juce;

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    StringArray args;
    for (int i = 1; i < argc; ++i) args.add (String (argv[i]).unquoted());
    if (args.isEmpty()) { std::printf ("usage: ParamDump plugin.vst3 [name] [points=N] [fs=48000]\n"); return 2; }

    int points = 21;
    double fs = 48000.0;
    String wanted;
    for (int i = 1; i < args.size(); ++i)
    {
        if (args[i].startsWith ("points=")) points = jmax (2, args[i].substring (7).getIntValue());
        else if (args[i].startsWith ("fs=")) fs = args[i].substring (3).getDoubleValue();
        else if (! args[i].startsWith ("state=")) wanted = args[i];
    }

    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());
    OwnedArray<PluginDescription> types;
    VST3PluginFormat vst3;
    vst3.findAllTypesForFile (types, args[0]);
    std::unique_ptr<AudioPluginInstance> plugin;
    String err;
    for (auto* t : types)
    {
        std::printf ("class: %s  vendor: %s  version: %s  uid: %d  ins: %d  outs: %d  category: %s\n",
                     t->name.toRawUTF8(), t->manufacturerName.toRawUTF8(), t->version.toRawUTF8(), t->uniqueId,
                     t->numInputChannels, t->numOutputChannels, t->category.toRawUTF8());
        if (plugin == nullptr && (wanted.isEmpty() || t->name == wanted))
            plugin = fm.createPluginInstance (*t, fs, 512, err);
    }
    if (plugin == nullptr) { std::printf ("load failed: %s\n", err.toRawUTF8()); return 1; }

    auto layout = plugin->getBusesLayout();
    for (int b = 0; b < layout.inputBuses.size(); ++b)
        std::printf ("input bus %d: %s\n", b, layout.inputBuses[b].getDescription().toRawUTF8());
    for (int b = 0; b < layout.outputBuses.size(); ++b)
        std::printf ("output bus %d: %s\n", b, layout.outputBuses[b].getDescription().toRawUTF8());
    std::printf ("mono->mono supported: %d\n",
                 (int) plugin->checkBusesLayoutSupported ({ { AudioChannelSet::mono() }, { AudioChannelSet::mono() } }));

    plugin->setPlayConfigDetails (2, 2, fs, 512);
    plugin->prepareToPlay (fs, 512);
    std::printf ("latency (samples @ %.0f): %d\n", fs, plugin->getLatencySamples());
    std::printf ("tail (s): %.3f\n", plugin->getTailLengthSeconds());
    std::printf ("programs: %d\n", plugin->getNumPrograms());
    for (int p = 0; p < plugin->getNumPrograms(); ++p)
        std::printf ("  program %d: %s\n", p, plugin->getProgramName (p).toRawUTF8());

    MemoryBlock state;
    plugin->getStateInformation (state);
    std::printf ("state bytes: %d\n", (int) state.getSize());
    for (int i = 1; i < args.size(); ++i)
        if (args[i].startsWith ("state="))
            File (args[i].substring (6)).replaceWithData (state.getData(), state.getSize());

    const auto& params = plugin->getParameters();
    std::printf ("parameters: %d\n", params.size());
    for (auto* p : params)
    {
        const float def = p->getDefaultValue();
        String id;
        if (auto* hp = dynamic_cast<HostedAudioProcessorParameter*> (p)) id = hp->getParameterID();
        std::printf ("\n[%d] id=%s name='%s' label='%s' steps=%d discrete=%d bool=%d automatable=%d meta=%d default=%.6f ('%s')\n",
                     p->getParameterIndex(), id.toRawUTF8(), p->getName (128).toRawUTF8(), p->getLabel().toRawUTF8(),
                     p->getNumSteps(), (int) p->isDiscrete(), (int) p->isBoolean(), (int) p->isAutomatable(),
                     (int) p->isMetaParameter(), def, p->getText (def, 128).toRawUTF8());
        const int n = (p->isDiscrete() && p->getNumSteps() > 1 && p->getNumSteps() <= 64) ? p->getNumSteps() : points;
        for (int k = 0; k < n; ++k)
        {
            const float v = (float) k / (float) (n - 1);
            std::printf ("    %.4f -> '%s'\n", v, p->getText (v, 128).toRawUTF8());
        }
        auto vals = p->getAllValueStrings();
        if (! vals.isEmpty()) std::printf ("    value strings: %s\n", vals.joinIntoString (" | ").toRawUTF8());
    }

    if (plugin->hasEditor())
    {
        std::unique_ptr<AudioProcessorEditor> ed (plugin->createEditorIfNeeded());
        if (ed != nullptr)
            std::printf ("\neditor: %d x %d  resizable: %d\n", ed->getWidth(), ed->getHeight(), (int) ed->isResizable());
        if (ed != nullptr) plugin->editorBeingDeleted (ed.get());
    }
    plugin->releaseResources();
    return 0;
}
