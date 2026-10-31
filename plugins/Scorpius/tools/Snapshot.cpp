// Renders the Scorpius editor to a PNG for the visual comparison with the reference (tools/compare_ui.py):
//   ScorpiusSnapshot out.png [--scale=2] [paramID=value ...] [--levels=inDb,outDb] [--audio] [--preset=name] [--slot=1]
//                   [--style=orion|scorpius] [--bench] [--film=audio.f32,seconds,every[,stopAt]]
// --film plays a file (float32, interleaved stereo, 48 kHz) through the processor and writes the stripe every
// <every> frames of the editor's 40 Hz clock as out_NNNN.png (out without its extension); from <stopAt> seconds on
// the processor is no longer called, as when a host stops.
// Parameter values are in real units (dB, 0 ... 1, Hz; driveColor 0 1 2, compMode 0 1); --levels freezes the I/O
// meters, --audio sets the animations as for that input level.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

int main (int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: ScorpiusSnapshot out.png [--scale=2] [paramID=value ...]" << std::endl; return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    auto proc = std::make_unique<ScorpiusProcessor>();
    float scale = 1.0f, inDb = -200.0f, outDb = -200.0f;
    bool audio = false;
    int slot = 0;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        const auto value = a.fromFirstOccurrenceOf ("=", false, false);
        if (a.startsWith ("--scale=")) { scale = value.getFloatValue(); continue; }
        if (a.startsWith ("--levels=")) { inDb = value.upToFirstOccurrenceOf (",", false, false).getFloatValue(); outDb = value.fromFirstOccurrenceOf (",", false, false).getFloatValue(); continue; }
        if (a == "--audio") { audio = true; continue; }
        if (a.startsWith ("--slot=")) { slot = value.getIntValue(); continue; }
        if (a.startsWith ("--style=")) { proc->particleStyle = value == "orion" ? 0 : 1; continue; }
        if (a.startsWith ("--preset="))
        {
            for (auto& e : proc->presets.list()) if (e.name == value) proc->presets.load (e);
            continue;
        }
        for (auto* p : proc->getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (r->getParameterID() == a.upToFirstOccurrenceOf ("=", false, false))
                    r->setValueNotifyingHost (r->convertTo0to1 (value.getFloatValue()));
    }
    if (slot == 1) { proc->copyCompareSlot(); proc->selectCompareSlot (1); }
    proc->uiScale = scale;
    std::unique_ptr<ScorpiusEditor> ed (dynamic_cast<ScorpiusEditor*> (proc->createEditor()));
    ed->setTestLevels (inDb, outDb, audio);         // (also reads the parameters, which the listeners do asynchronously)
    for (int i = 2; i < argc; ++i)
        if (juce::String (argv[i]) == "--bench")         // how long the editor's paints take (ms per paint)
        {
            juce::Image target (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
            auto time = [&] (const char* what, juce::Rectangle<int> area, int runs)
            {
                const double t0 = juce::Time::getMillisecondCounterHiRes();
                for (int r = 0; r < runs; ++r)
                {
                    juce::Graphics g (target);
                    g.reduceClipRegion (area);
                    ed->paintEntireComponent (g, true);
                }
                std::cout << what << ": " << (juce::Time::getMillisecondCounterHiRes() - t0) / runs << " ms" << std::endl;
            };
            const float s = scale;
            time ("whole editor", ed->getLocalBounds(), 20);
            time ("input meters", juce::Rectangle<float> (0, 36, 86, 236).transformedBy (juce::AffineTransform::scale (s)).toNearestInt(), 100);
            time ("harmonics circle", juce::Rectangle<float> (195, 74, 50, 50).transformedBy (juce::AffineTransform::scale (s)).toNearestInt(), 100);
            time ("stripe", juce::Rectangle<float> (10, 272, 868, 36).transformedBy (juce::AffineTransform::scale (s)).toNearestInt(), 100);
            return 0;
        }
    for (int i = 2; i < argc; ++i)
        if (juce::String (argv[i]).startsWith ("--film="))
        {
            const auto spec = juce::StringArray::fromTokens (juce::String (argv[i]).fromFirstOccurrenceOf ("=", false, false), ",", "");
            juce::MemoryBlock raw;
            juce::File (spec[0]).loadFileAsData (raw);
            const auto* audio = static_cast<const float*> (raw.getData());
            const int frames = (int) (raw.getSize() / 8);
            const float seconds = spec[1].getFloatValue();
            const int every = juce::jmax (1, spec[2].getIntValue());
            const float stopAt = spec.size() > 3 ? spec[3].getFloatValue() : 1.0e9f;
            proc->setRateAndBufferSizeDetails (48000.0, 300);
            proc->prepareToPlay (48000.0, 300);
            juce::AudioBuffer<float> buf (2, 300);
            juce::MidiBuffer midi;
            const auto base = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]).withFileExtension ("");
            int pos = 0;
            const juce::Rectangle<int> area = juce::Rectangle<float> (10, 272, 868, 36).transformedBy (juce::AffineTransform::scale (scale)).toNearestInt();
            for (int tick = 0; tick < (int) (seconds * 40.0f) && pos + 1200 <= frames; ++tick)
            {
                const bool stopped = (float) tick / 40.0f >= stopAt;
                for (int b = 0; b < 4 && ! stopped; ++b, pos += 300)
                {
                    for (int k = 0; k < 300; ++k)
                    {
                        buf.setSample (0, k, audio[2 * (pos + k)]);
                        buf.setSample (1, k, audio[2 * (pos + k) + 1]);
                    }
                    proc->processBlock (buf, midi);
                }
                ed->filmStep (tick / 40.0, ! stopped);
                if (tick % every != 0) continue;
                auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f).getClippedImage (area);
                auto f = base.getSiblingFile (base.getFileName() + juce::String::formatted ("_%04d.png", tick));
                f.deleteFile();
                juce::FileOutputStream os (f);
                juce::PNGImageFormat().writeImageToStream (img, os);
            }
            return 0;
        }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    ed.reset();
    return 0;
}
