// RefRender: offline host for the measurements (Phase 3). Renders stereo float32 files through a VST3.
//   RefRender plugin.vst3 "name" jobs.txt [fs=48000] [block=512]
// jobs.txt: one render per line:  in.f32 out.f32 [key=value ...]
//   param:Name=text      set a parameter by its displayed value (e.g. param:Character=0.25)
//   norm:Name=0.5        set a parameter by its normalised value
//   auto:Name@sample=0.7 change a parameter (normalised) at an input sample position (smoothing tests)
//   state=file           load a raw plug-in state before the parameters
//   sc=file.f32          side-chain input (stereo interleaved)
//   raw=1                no latency compensation (the output is the plug-in's output sample for sample)
//   reset=0              keep the instance of the previous job (default: a fresh instance per job)
// Prints the latency and, per job, the number of frames written.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>
#include <map>
#ifdef _WIN32
 #include <windows.h>
#endif

using namespace juce;

static std::vector<float> readF32 (const String& path)
{
    MemoryBlock mb;
    File (File::getCurrentWorkingDirectory().getChildFile (path)).loadFileAsData (mb);
    return std::vector<float> ((const float*) mb.getData(), (const float*) mb.getData() + mb.getSize() / sizeof (float));
}

// runs the message loop for a while (this host has no event loop of its own)
static void pumpMessages (int ms)
{
    const auto end = Time::getMillisecondCounter() + (uint32) ms;
    while (Time::getMillisecondCounter() < end)
    {
       #ifdef _WIN32
        MSG m;
        while (PeekMessage (&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage (&m); DispatchMessage (&m); }
       #endif
        Thread::sleep (5);
    }
}

struct FixedHead : AudioPlayHead
{
    int64 pos = 0; double fs = 48000.0;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setTimeInSamples (pos);
        p.setTimeInSeconds ((double) pos / fs);
        p.setIsPlaying (true);
        p.setBpm (120.0);
        return p;
    }
};

static AudioProcessorParameter* findParam (AudioPluginInstance& p, const String& name)
{
    for (auto* q : p.getParameters())
        if (q->getName (128).equalsIgnoreCase (name)) return q;
    return nullptr;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    StringArray args;
    for (int i = 1; i < argc; ++i) args.add (String (argv[i]).unquoted());
    if (args.size() < 3) { std::printf ("usage: RefRender plugin.vst3 name jobs.txt [fs=48000] [block=512]\n"); return 2; }
    double fs = 48000.0;
    int block = 512;
    for (int i = 3; i < args.size(); ++i)
    {
        if (args[i].startsWith ("fs=")) fs = args[i].substring (3).getDoubleValue();
        if (args[i].startsWith ("block=")) block = args[i].substring (6).getIntValue();
    }

    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());
    OwnedArray<PluginDescription> types;
    VST3PluginFormat vst3;
    vst3.findAllTypesForFile (types, args[0]);
    PluginDescription* desc = nullptr;
    for (auto* t : types)
        if (t->name == args[1]) desc = t;
    if (desc == nullptr) { std::printf ("plug-in not found\n"); return 1; }
    FixedHead head;
    head.fs = fs;
    int nIn = 0, nOut = 0;
    bool verbose = true;
    // a fresh instance per job: resetting through the plug-in's state is not reliable (the reference applies its
    // state asynchronously, after the parameters set here)
    auto create = [&]
    {
        String err;
        auto p = fm.createPluginInstance (*desc, fs, block, err);
        if (p == nullptr) { std::printf ("load failed: %s\n", err.toRawUTF8()); return p; }
        auto layout = p->getBusesLayout();                   // main stereo; the side-chain bus in stereo (or mono) when present
        for (int b = 1; b < layout.inputBuses.size(); ++b) layout.inputBuses.getReference (b) = AudioChannelSet::stereo();
        if (! p->setBusesLayout (layout))
        {
            for (int b = 1; b < layout.inputBuses.size(); ++b) layout.inputBuses.getReference (b) = AudioChannelSet::mono();
            if (! p->setBusesLayout (layout)) std::printf ("side-chain layout refused\n");
        }
        for (int b = 0; b < p->getBusCount (true); ++b)
            if (verbose) std::printf ("  in bus %d '%s': %d ch, enabled %d\n", b, p->getBus (true, b)->getName().toRawUTF8(),
                                      p->getBus (true, b)->getNumberOfChannels(), (int) p->getBus (true, b)->isEnabled());
        verbose = false;
        nIn = p->getTotalNumInputChannels(); nOut = p->getTotalNumOutputChannels();
        p->setPlayConfigDetails (nIn, nOut, fs, block);
        p->setPlayHead (&head);
        return p;
    };
    auto plugin = create();
    if (plugin == nullptr) return 1;
    plugin->prepareToPlay (fs, block);
    std::printf ("loaded %s: in %d ch, out %d ch, latency %d\n", plugin->getName().toRawUTF8(), nIn, nOut, plugin->getLatencySamples());

    StringArray lines;
    File (File::getCurrentWorkingDirectory().getChildFile (args[2])).readLines (lines);
    for (auto& line : lines)
    {
        auto tok = StringArray::fromTokens (line, " ", "\"");
        tok.removeEmptyStrings();
        if (tok.size() < 2) continue;
        for (auto& t : tok) t = t.unquoted();
        String scPath, statePath;
        bool raw = false, reset = true;
        std::multimap<int, std::pair<AudioProcessorParameter*, float>> autom;
        for (int i = 2; i < tok.size(); ++i)
        {
            const auto& a = tok[i];
            if (a.startsWith ("sc=")) scPath = a.substring (3);
            else if (a.startsWith ("state=")) statePath = a.substring (6);
            else if (a == "raw=1") raw = true;
            else if (a == "reset=0") reset = false;
        }
        if (reset)
        {
            plugin->releaseResources();
            plugin.reset();
            plugin = create();
            if (plugin == nullptr) return 1;
            if (statePath.isNotEmpty())
            {
                MemoryBlock mb;
                File (File::getCurrentWorkingDirectory().getChildFile (statePath)).loadFileAsData (mb);
                plugin->setStateInformation (mb.getData(), (int) mb.getSize());
                pumpMessages (300);                                 // let a plug-in finish applying it asynchronously
            }
        }
        for (int i = 2; i < tok.size(); ++i)
        {
            const auto& a = tok[i];
            const bool isParam = a.startsWith ("param:"), isNorm = a.startsWith ("norm:"), isAuto = a.startsWith ("auto:");
            if (! (isParam || isNorm || isAuto)) continue;
            const auto body = a.fromFirstOccurrenceOf (":", false, false);
            auto name = body.upToFirstOccurrenceOf ("=", false, false);
            const auto val = body.fromFirstOccurrenceOf ("=", false, false);
            int at = -1;
            if (isAuto) { at = name.fromFirstOccurrenceOf ("@", false, false).getIntValue(); name = name.upToFirstOccurrenceOf ("@", false, false); }
            auto* p = findParam (*plugin, name);
            if (p == nullptr) { std::printf ("  no parameter '%s'\n", name.toRawUTF8()); continue; }
            const float v = isParam ? p->getValueForText (val) : val.getFloatValue();
            if (isAuto) autom.insert ({ at, { p, v } });
            else p->setValueNotifyingHost (v);
        }
        if (reset) plugin->prepareToPlay (fs, block);

        const auto in = readF32 (tok[0]);
        const auto sc = scPath.isNotEmpty() ? readF32 (scPath) : std::vector<float>();
        const int latency = raw ? 0 : plugin->getLatencySamples();
        const int n = (int) (in.size() / 2);
        std::vector<float> out ((size_t) n * 2, 0.0f);
        AudioBuffer<float> buf (jmax (nIn, nOut), block);
        MidiBuffer midi;
        auto next = autom.begin();
        for (int pos = 0; pos < n + latency; pos += block)
        {
            // parameter changes land on block boundaries (512 = 10.7 ms at 48 kHz unless block= is smaller)
            while (next != autom.end() && next->first < pos + block) { next->second.first->setValueNotifyingHost (next->second.second); ++next; }
            buf.clear();
            for (int i = 0; i < block; ++i)
            {
                const int k = pos + i;
                if (k < n)
                {
                    buf.setSample (0, i, in[(size_t) k * 2]);
                    buf.setSample (1, i, in[(size_t) k * 2 + 1]);
                    if (nIn > 3 && (size_t) k * 2 + 1 < sc.size()) { buf.setSample (2, i, sc[(size_t) k * 2]); buf.setSample (3, i, sc[(size_t) k * 2 + 1]); }
                }
            }
            head.pos = pos;
            plugin->processBlock (buf, midi);
            for (int i = 0; i < block; ++i)
            {
                const int k = pos + i - latency;
                if (k >= 0 && k < n)
                {
                    out[(size_t) k * 2] = buf.getSample (0, i);
                    out[(size_t) k * 2 + 1] = buf.getSample (1, i);
                }
            }
        }
        File f (File::getCurrentWorkingDirectory().getChildFile (tok[1]));
        f.deleteFile();
        FileOutputStream os (f);
        os.write (out.data(), out.size() * sizeof (float));
        std::printf ("job %s: %d frames, latency %d\n", tok[1].toRawUTF8(), n, latency);
        std::fflush (stdout);
    }
    plugin->releaseResources();
    return 0;
}
