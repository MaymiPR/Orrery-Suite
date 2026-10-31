#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace sc
{
namespace
{
// the reference shows every value with three decimals ("0.800", "-12.000", "80.000")
String threeDecimals (float v, int) { return String (v, 3); }
}

AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto attrs = AudioParameterFloatAttributes().withStringFromValueFunction (threeDecimals);
    auto gain = [&] (const char* pid, const char* name)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, NormalisableRange<float> (-12.0f, 12.0f), 0.0f, attrs));
    };
    auto onOff = [&] (const char* pid, const char* name, bool on)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 1.0f), on ? 1.0f : 0.0f, attrs));
    };
    auto amount = [&] (const char* pid, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def, attrs));
    };
    gain (id::inputGain, "Input Gain");
    onOff (id::driveOn, "Drive On/Off", true);
    onOff (id::driveHiPass, "Drive Hi-Pass", true);
    onOff (id::harmOn, "Harm On/Off", true);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::harmFreq, 1 }, "Harm Freq", NormalisableRange<float> (50.0f, 80.0f, 30.0f), 80.0f, attrs));
    amount (id::harmAmount, "Harm Amount", 0.8f);
    onOff (id::compOn, "Comp On/Off", true);
    amount (id::compAmount, "Comp Amount", 0.9f);
    onOff (id::liftOn, "Lift On/Off", true);
    amount (id::liftAmount, "Lift Amount", 0.8f);
    onOff (id::eqOn, "EQ On/Off", false);
    const char* eqNames[5] { "EQ 40Hz", "EQ 100Hz", "EQ 1kHz", "EQ 3kHz", "EQ 10kHz" };
    for (int b = 0; b < 5; ++b)
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { eqIds[b], 1 }, eqNames[b], NormalisableRange<float> (0.0f, 6.0f), 0.0f, attrs));
    const char* parNames[4][2] { { "Par Sub On/Off", "Par Sub Level" }, { "Par Low On/Off", "Par Low Level" },
                                 { "Par Loud On/Off", "Par Loud Level" }, { "Par Grit On/Off", "Par Grit Level" } };
    for (int k = 0; k < 4; ++k)
    {
        onOff (parallels[k].on, parNames[k][0], false);
        amount (parallels[k].level, parNames[k][1], 0.0f);
    }
    gain (id::outputGain, "Output Gain");
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::driveColor, 1 }, "Drive Color", StringArray { "a", "b", "c" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::compMode, 1 }, "Comp Mode", StringArray { "a", "b" }, 1));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, 1 }, "Bypass", false));
    return layout;
}
}

ScorpiusProcessor::ScorpiusProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "Scorpius", sc::createLayout())
{
    bypassParam = apvts.getParameter (sc::id::bypass);
    inGainRaw = apvts.getRawParameterValue (sc::id::inputGain);
    outGainRaw = apvts.getRawParameterValue (sc::id::outputGain);
    compAmountRaw = apvts.getRawParameterValue (sc::id::compAmount);
    slots[0] = slots[1] = captureSlot();
}

bool ScorpiusProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    // stereo only, like the reference (mono -> mono is refused); the side-chain bus (ours) is stereo or off
    if (l.getMainInputChannelSet() != AudioChannelSet::stereo() || l.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;
    if (l.inputBuses.size() > 1)
    {
        const auto side = l.inputBuses[1];
        if (! side.isDisabled() && side != AudioChannelSet::stereo()) return false;
    }
    return true;
}

void ScorpiusProcessor::prepareToPlay (double sampleRate, int block)
{
    meterBuf.setSize (2, jmax (1, block));
    inMeter.prepare (sampleRate);
    outMeter.prepare (sampleRate);
    visuals.prepare (sampleRate);
    light.prepare (sampleRate);
    kick.prepare (sampleRate);
    // the modules run at 48 kHz (their fits and learned models are made there); other rates are converted in and out,
    // as the reference does (MEASUREMENTS.md 2)
    resampling = std::abs (sampleRate - 48000.0) > 0.5;
    ownConverters = sc::dsp::fits::convertersAt (sampleRate);
    const int block48 = (int) std::ceil (jmax (1, block) * 48000.0 / sampleRate) + 8;
    engine.prepare (48000.0, block48);
    silence.assign ((size_t) block48, 0.0f);
    if (resampling)
    {
        for (int c = 0; c < 2; ++c)
        {
            const auto& t = sc::dsp::tuning();
            const bool low = sampleRate < 48000.0;
            const int hIn = low ? 16 : (int) std::lround (t.resampleHalfInHigh), hOut = low ? 16 : (int) std::lround (t.resampleHalfOutHigh);
            const double beta = low ? 8.0 : t.resampleBetaHigh;
            toInner[c].prepare (sampleRate, 48000.0, low ? t.resampleCutoffIn : t.resampleCutoffInHigh, hIn, beta);
            keyToInner[c].prepare (sampleRate, 48000.0, low ? t.resampleCutoffIn : t.resampleCutoffInHigh, hIn, beta);
            toOuter[c].prepare (48000.0, sampleRate, low ? t.resampleCutoffOut : t.resampleCutoffOutHigh, hOut, beta);
            if (const auto* f = ownConverters)  // the reference's own converters, identified (exact delay, no hold-back)
            {
                toInner[c].prepareRational (f->p, f->q, true, *f->down);
                keyToInner[c].prepareRational (f->p, f->q, true, *f->down);
                toOuter[c].prepareRational (f->p, f->q, false, *f->up);
            }
            inner[c].reserve ((size_t) block48 * 2);
            keyInner[c].reserve ((size_t) block48 * 2);
        }
        // the delay the reference has at this rate (measured; reported a little short above 48 kHz, as it does):
        // a few samples of output held ahead (so the output never runs short), the rest trimmed by the converters'
        // start offsets
        auto [target, reported] = referenceDelay (sampleRate);
        if (ownConverters != nullptr)
        {
            for (int c = 0; c < 2; ++c) outer[c].clear();
            setLatencySamples (reported);
        }
        else
        {
        if (sampleRate > 48000.0) target += sc::dsp::tuning().resampleDelayTrimHigh;
        const double r = sampleRate / 48000.0, full = toInner[0].halfLength() + toOuter[0].halfLength() * r;
        const int prefill = jmax (2, (int) std::ceil (target - full));
        const double trim = full + prefill - target;                  // 0 ... 1 + r
        const double inTrim = jmin (0.999, trim), outTrim = jmin (0.999, (trim - inTrim) / r);
        for (int c = 0; c < 2; ++c)
        {
            toInner[c].setOffset (inTrim); keyToInner[c].setOffset (inTrim); toOuter[c].setOffset (outTrim);
            outer[c].assign ((size_t) prefill, 0.0f);
        }
        setLatencySamples (reported);
        }
    }
    else setLatencySamples (0);
    // the input and output gains glide exponentially (measured: 90 % of a step after 19 ms, tau ~8.3 ms)
    gainCoef = (float) std::exp (-1.0 / (0.0083 * sampleRate));
    inGainNow = Decibels::decibelsToGain (inGainRaw->load());
    outGainNow = Decibels::decibelsToGain (outGainRaw->load());
}

void ScorpiusProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    if (auto* head = getPlayHead())
        if (const auto position = head->getPosition())
        {
            if (const auto bpm = position->getBpm()) hostBpm = (float) *bpm;
            hostPlaying = position->getIsPlaying() ? 1 : 0;
        }
    auto main = getBusBuffer (buffer, true, 0);
    const int n = main.getNumSamples();
    if (main.getNumChannels() < 2) return;
    const bool bypassed = bypassParam->getValue() >= 0.5f;
    // EXT: the compressor listens to the side-chain bus (silence when the host leaves the bus off)
    const bool external = sidechainExternal.load();
    AudioBuffer<float> side;
    if (external && getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto bus = getBusBuffer (buffer, true, 1);
        if (bus.getNumChannels() >= 2) side = AudioBuffer<float> (bus.getArrayOfWritePointers(), 2, n);
    }

    const float inDb = inGainRaw->load(), outDb = ioLink.load() ? -inDb : outGainRaw->load();   // the I/O link: output = -input, always
    const float inTarget = bypassed ? 1.0f : Decibels::decibelsToGain (inDb), outTarget = bypassed ? 1.0f : Decibels::decibelsToGain (outDb);
    for (int i = 0; i < n; ++i)
    {
        inGainNow = inTarget + (inGainNow - inTarget) * gainCoef;
        const float g = inGainNow;
        main.getWritePointer (0)[i] *= g;
        main.getWritePointer (1)[i] *= g;
    }
    // the input meters read the signal after the input gain
    inMeter.measure (main);
    visuals.process (main);
    light.process (main);
    kick.process (main);
    const bool key = external && side.getNumChannels() >= 2;
    if (! resampling)
    {
        if (! bypassed) runEngine (main.getWritePointer (0), main.getWritePointer (1), n,
                                   key ? side.getReadPointer (0) : (external ? silence.data() : nullptr),
                                   key ? side.getReadPointer (1) : (external ? silence.data() : nullptr));
    }
    else
    {
        // to 48 kHz, the modules, back to the host's rate (the conversion runs while bypassed too, so the latency holds)
        for (int c = 0; c < 2; ++c)
        {
            inner[c].clear(); keyInner[c].clear();
            toInner[c].process (main.getReadPointer (c), n, inner[c]);
            if (key) keyToInner[c].process (side.getReadPointer (c), n, keyInner[c]);
        }
        const int m = (int) inner[0].size();
        if (silence.size() < (size_t) m) silence.resize ((size_t) m, 0.0f);
        if (! bypassed) runEngine (inner[0].data(), inner[1].data(), m,
                                   key ? keyInner[0].data() : (external ? silence.data() : nullptr),
                                   key ? keyInner[1].data() : (external ? silence.data() : nullptr));
        for (int c = 0; c < 2; ++c)
        {
            toOuter[c].process (inner[c].data(), m, outer[c]);
            const int take = jmin (n, (int) outer[c].size());
            std::copy (outer[c].begin(), outer[c].begin() + take, main.getWritePointer (c));
            std::fill (main.getWritePointer (c) + take, main.getWritePointer (c) + n, 0.0f);
            outer[c].erase (outer[c].begin(), outer[c].begin() + take);
        }
    }
    for (int i = 0; i < n; ++i)
    {
        outGainNow = outTarget + (outGainNow - outTarget) * gainCoef;
        const float g = outGainNow;
        main.getWritePointer (0)[i] *= g;
        main.getWritePointer (1)[i] *= g;
    }
    outMeter.measure (main);
    compGr = (float) engine.compReadout();    // the readout's own ballistics (dsp/Compressor.h)
    ++blocksProcessed;
}

// the reference's delay (samples) at a rate, and the latency it reports (MEASUREMENTS.md 2; between the measured
// rates, linear in the rate)
std::pair<double, int> ScorpiusProcessor::referenceDelay (double fs)
{
    static constexpr double rate[] { 32000, 44100, 48000, 88200, 96000, 176400, 192000 };
    static constexpr double delay[] { 27.0, 31.0, 0.0, 48.742, 51.1, 81.461, 87.085 };
    static constexpr double shown[] { 27, 31, 0, 46, 48, 75, 80 };
    if (std::abs (fs - 48000.0) < 0.5) return { 0.0, 0 };
    // (48 kHz itself has none: interpolate between its neighbours that convert)
    std::vector<std::pair<double, std::pair<double, double>>> pts;
    for (int i = 0; i < 7; ++i) if (! juce::exactlyEqual (rate[i], 48000.0)) pts.push_back ({ rate[i], { delay[i], shown[i] } });
    if (fs <= pts.front().first) return { pts.front().second.first * fs / pts.front().first, (int) std::lround (pts.front().second.second * fs / pts.front().first) };
    for (size_t i = 1; i < pts.size(); ++i)
        if (fs <= pts[i].first)
        {
            const double u = (fs - pts[i - 1].first) / (pts[i].first - pts[i - 1].first);
            return { pts[i - 1].second.first + u * (pts[i].second.first - pts[i - 1].second.first),
                     (int) std::lround (pts[i - 1].second.second + u * (pts[i].second.second - pts[i - 1].second.second)) };
        }
    const auto& l = pts.back();
    return { l.second.first * fs / l.first, (int) std::lround (l.second.second * fs / l.first) };
}

// the modules over a block (at 48 kHz), in pieces no longer than the engine was prepared for
void ScorpiusProcessor::runEngine (float* l, float* r, int n, const float* keyL, const float* keyR)
{
    const auto params = engineParams();
    const int most = jmax (1, (int) silence.size() - 8);
    for (int pos = 0; pos < n; )
    {
        const int len = jmin (n - pos, most);
        engine.process (l + pos, r + pos, len, params, keyL != nullptr ? keyL + pos : nullptr, keyR != nullptr ? keyR + pos : nullptr);
        pos += len;
    }
}

sc::dsp::Params ScorpiusProcessor::engineParams() const
{
    using namespace sc;
    sc::dsp::Params p;
    auto on = [this] (const char* pid) { return raw (pid)->load() >= 0.5f; };
    p.driveOn = on (id::driveOn); p.hiPass = on (id::driveHiPass); p.driveColor = (int) raw (id::driveColor)->load();
    p.harmOn = on (id::harmOn); p.harmFreq = raw (id::harmFreq)->load() < 65.0f ? 50 : 80; p.harmAmount = raw (id::harmAmount)->load();
    p.compOn = on (id::compOn); p.compMode = (int) raw (id::compMode)->load(); p.compAmount = raw (id::compAmount)->load();
    p.liftOn = on (id::liftOn); p.liftAmount = raw (id::liftAmount)->load();
    p.eqOn = on (id::eqOn);
    for (int b = 0; b < 5; ++b) p.eq[b] = raw (eqIds[b])->load();
    for (int k = 0; k < 4; ++k) { p.parOn[k] = on (parallels[k].on); p.parLevel[k] = raw (parallels[k].level)->load(); }
    return p;
}

//==============================================================================
ValueTree ScorpiusProcessor::captureSlot() const
{
    ValueTree t ("SLOT");
    for (auto* pid : sc::presetIds())
        if (auto* p = apvts.getParameter (pid)) t.setProperty (pid, p->getValue(), nullptr);
    t.setProperty ("preset", presets.currentName(), nullptr);
    return t;
}

void ScorpiusProcessor::applySlot (const ValueTree& t)
{
    for (auto* pid : sc::presetIds())
        if (auto* p = apvts.getParameter (pid))
            if (t.hasProperty (pid))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost ((float) t.getProperty (pid));
                p->endChangeGesture();
            }
    presets.setCurrentName (t.getProperty ("preset", presets.currentName()).toString());
}

void ScorpiusProcessor::selectCompareSlot (int s)
{
    s = jlimit (0, 1, s);
    if (s == slot) return;
    slots[slot] = captureSlot();
    if (! slotUsed[s]) slots[s] = slots[slot].createCopy();
    slotUsed[s] = true;
    slot = s;
    applySlot (slots[slot]);
}

void ScorpiusProcessor::copyCompareSlot()
{
    slots[slot] = captureSlot();
    slots[1 - slot] = slots[slot].createCopy();
    slotUsed[1 - slot] = true;
}

void ScorpiusProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    state.setProperty ("animation", animation.load() ? 1 : 0, nullptr);
    state.setProperty ("particleStyle", particleStyle.load(), nullptr);
    state.setProperty ("preset", presets.currentName(), nullptr);
    state.setProperty ("compare", slot, nullptr);
    state.setProperty ("ioLink", ioLink.load() ? 1 : 0, nullptr);
    state.setProperty ("sidechain", sidechainExternal.load() ? 1 : 0, nullptr);
    // the inactive compare slot (the active one is the parameters themselves), if it was ever used
    state.removeChild (state.getChildWithName ("SLOT"), nullptr);
    if (slotUsed[1 - slot]) state.appendChild (slots[1 - slot].createCopy(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ScorpiusProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            uiScale = (float) tree.getProperty ("uiScale", 1.0f);
            animation = (int) tree.getProperty ("animation", 1) != 0;
            particleStyle = jlimit (0, 1, (int) tree.getProperty ("particleStyle", 1));
            presets.setCurrentName (tree.getProperty ("preset", sc::PresetManager::defaultName()).toString());
            slot = jlimit (0, 1, (int) tree.getProperty ("compare", 0));
            ioLink = (int) tree.getProperty ("ioLink", 0) != 0;
            sidechainExternal = (int) tree.getProperty ("sidechain", 0) != 0;
            auto other = tree.getChildWithName ("SLOT");
            tree.removeChild (other, nullptr);
            apvts.replaceState (tree);
            slots[slot] = captureSlot();
            slotUsed[slot] = true;
            slotUsed[1 - slot] = other.isValid();
            slots[1 - slot] = other.isValid() ? other.createCopy() : slots[slot].createCopy();
        }
}

AudioProcessorEditor* ScorpiusProcessor::createEditor() { return new ScorpiusEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ScorpiusProcessor(); }
