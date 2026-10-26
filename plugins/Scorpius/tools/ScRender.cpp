// ScRender: offline renders through Scorpius's own processor (no plug-in loading), with every DSP constant
// open to change. Same job format as RefRender, so nulltest/orion.py drives both:
//   ScRender jobs.txt [fs=48000] [block=512] [tuning=file]
// jobs.txt: one render per line:  in.f32 out.f32 [key=value ...]
//   param:Name=text  norm:Name=0.5  auto:Name@sample=0.7  sc=file.f32  raw=1   (as RefRender)
//   tune:key=value   a Tuning constant for this job (Source/dsp/Tuning.h), e.g. tune:eqCeiling=0.45
//   ext=1            side-chain EXT (the host-only setting)
#include <cstdio>
#include <map>
#include "PluginProcessor.h"

using namespace juce;

static std::vector<float> readF32 (const String& path)
{
    MemoryBlock mb;
    File (File::getCurrentWorkingDirectory().getChildFile (path)).loadFileAsData (mb);
    return std::vector<float> ((const float*) mb.getData(), (const float*) mb.getData() + mb.getSize() / sizeof (float));
}

static AudioProcessorParameter* findParam (AudioProcessor& p, const String& name)
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
    if (args.size() < 1) { std::printf ("usage: ScRender jobs.txt [fs=48000] [block=512] [tuning=file]\n"); return 2; }
    double fs = 48000.0;
    int block = 512;
    const sc::dsp::Tuning defaults = sc::dsp::tuning();
    sc::dsp::Tuning base = defaults;
    for (int i = 1; i < args.size(); ++i)
    {
        if (args[i].startsWith ("fs=")) fs = args[i].substring (3).getDoubleValue();
        if (args[i].startsWith ("block=")) block = args[i].substring (6).getIntValue();
        if (args[i].startsWith ("tuning=") && base.load (args[i].substring (7).toRawUTF8()) != 0) return 1;
    }

    StringArray lines;
    File (File::getCurrentWorkingDirectory().getChildFile (args[0])).readLines (lines);
    for (auto& line : lines)
    {
        auto tok = StringArray::fromTokens (line, " ", "\"");
        tok.removeEmptyStrings();
        if (tok.size() < 2) continue;
        for (auto& t : tok) t = t.unquoted();

        sc::dsp::tuning() = base;
        String scPath;
        bool raw = false, ext = false;
        for (int i = 2; i < tok.size(); ++i)
        {
            const auto& a = tok[i];
            if (a.startsWith ("sc=")) scPath = a.substring (3);
            else if (a == "raw=1") raw = true;
            else if (a == "ext=1") ext = true;
            else if (a.startsWith ("tune:"))
            {
                const auto kv = a.substring (5);
                if (! sc::dsp::tuning().set (kv.upToFirstOccurrenceOf ("=", false, false).toStdString(), kv.fromFirstOccurrenceOf ("=", false, false).getDoubleValue()))
                    { std::printf ("unknown tuning key in '%s'\n", a.toRawUTF8()); return 1; }
            }
        }

        ScorpiusProcessor proc;
        auto layout = proc.getBusesLayout();
        if (layout.inputBuses.size() > 1) layout.inputBuses.getReference (1) = AudioChannelSet::stereo();
        proc.setBusesLayout (layout);
        proc.sidechainExternal = ext;
        std::multimap<int, std::pair<AudioProcessorParameter*, float>> autom;
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
            auto* p = findParam (proc, name);
            if (p == nullptr) { std::printf ("  no parameter '%s'\n", name.toRawUTF8()); continue; }
            const float v = isParam ? p->getValueForText (val) : val.getFloatValue();
            if (isAuto) autom.insert ({ at, { p, v } });
            else p->setValueNotifyingHost (v);
        }
        const int nIn = proc.getTotalNumInputChannels();
        proc.setRateAndBufferSizeDetails (fs, block);    // (setPlayConfigDetails would reset the side-chain bus)
        proc.prepareToPlay (fs, block);

        const auto in = readF32 (tok[0]);
        const auto sc = scPath.isNotEmpty() ? readF32 (scPath) : std::vector<float>();
        const int latency = raw ? 0 : proc.getLatencySamples();
        const int n = (int) (in.size() / 2);
        std::vector<float> out ((size_t) n * 2, 0.0f);
        AudioBuffer<float> buf (jmax (nIn, 2), block);
        MidiBuffer midi;
        auto next = autom.begin();
        for (int pos = 0; pos < n + latency; pos += block)
        {
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
            proc.processBlock (buf, midi);
            for (int i = 0; i < block; ++i)
            {
                const int k = pos + i - latency;
                if (k >= 0 && k < n) { out[(size_t) k * 2] = buf.getSample (0, i); out[(size_t) k * 2 + 1] = buf.getSample (1, i); }
            }
        }
        File f (File::getCurrentWorkingDirectory().getChildFile (tok[1]));
        f.deleteFile();
        FileOutputStream os (f);
        os.write (out.data(), out.size() * sizeof (float));
        std::printf ("job %s: %d frames, latency %d\n", tok[1].toRawUTF8(), n, latency);
        std::fflush (stdout);
    }
    sc::dsp::tuning() = defaults;
    return 0;
}
