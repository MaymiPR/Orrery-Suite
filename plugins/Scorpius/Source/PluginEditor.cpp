#include "PluginEditor.h"
#include "shared/ui/KnobShadow.h"
#include "shared/ui/SaveDialog.h"

using namespace juce;
namespace theme = orrery::theme;
#ifndef SUITE_REPOSITORY
 #define SUITE_REPOSITORY "https://github.com/MaymiPR/Orrery-Suite"
#endif
static const juce::String repository = SUITE_REPOSITORY;     // the Orrery Suite repository (CMakeLists.txt)

namespace
{
// Layout measured on the reference at 200 % (plugins/Scorpius/tools/measure_ui.py, measure_text.py), design px.
constexpr float columnX[] { 86.0f, 175.0f, 424.0f, 608.0f, 801.0f };
constexpr float panelTop = 31.75f, panelBottom = 308.0f, plateTop = 310.75f;
constexpr float dividerTop = 36.0f, dividerBottom = 272.5f, stripeLine = 272.5f;
constexpr float labelBaseline = 263.5f, upperLabelBaseline = 192.5f, selectorLabelBaseline = 194.0f;
constexpr float readoutCentres[2][2] { { 30.5f, 60.5f }, { 827.5f, 857.5f } };     // input L R, output L R
constexpr float ledX[2][2] { { 26.0f, 56.0f }, { 824.0f, 854.0f } };
constexpr float ledBottom = 258.0f;
// knob centres and radii (silver regions corrected as on The God Particle, whose knobs are known exactly)
// (then checked against our own snapshot: the column knobs +0.5 / +0.8, the plate knobs +0.47 / +0.52)
constexpr float smallR = 20.11f, plateR = 25.06f, bottomRowY = 229.76f, topRowY = 158.68f, plateKnobY = 356.6f;
constexpr float harmX = 220.25f, compX = 300.24f, liftX = 380.25f;
constexpr float eqX[5] { 453.4f, 481.4f, 517.4f, 553.4f, 580.4f };       // 40, 100 (top row), 1k, 3k (top row), 10k
constexpr bool eqTop[5] { false, true, false, true, false };
constexpr float eqLabelX[5] { 453.0f, 480.5f, 516.5f, 553.25f, 580.0f };
constexpr float parX[4] { 636.0f, 682.0f, 728.0f, 774.0f };
constexpr float inputX = 46.235f, outputX = 842.22f;

// fractional bounds: the component is placed with a transform so it lands on the measured position
void place (Component& c, float x, float y, float w, float h)
{
    c.setBounds (0, 0, (int) std::ceil (w), (int) std::ceil (h));
    c.setTransform (AffineTransform::translation (x, y));
}
void placeKnob (Component& k, float cx, float cy, float r)
{
    const int s = (int) std::ceil (2.0f * r);
    k.setBounds (0, 0, s, s);
    k.setTransform (AffineTransform::scale (2.0f * r / (float) s).translated (cx - r, cy - r));
}

String grText (float gr)
{
    // the reference writes a reduction with a gap after the minus ("- 3.6") and rest as "0.0"
    if (gr < 0.05f) return "0.0";
    return "- " + String (gr, 1);
}
}

ScorpiusEditor::ScorpiusEditor (ScorpiusProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    orrery::noFocus (*this);
    orrery::noFocus (content);
    setOpaque (true);
    addAndMakeVisible (content);
    content.setBounds (0, 0, (int) std::ceil (designW), (int) std::ceil (designH));

    for (auto* c : std::initializer_list<Component*> { &logo, &presetBox, &prevButton, &nextButton, &saveButton, &compare, &sidechain, &ioLinkButton, &scaleBox, &gear,
                                                       &drivePower, &sparks, &hiPass, &color, &harmPower, &compPower, &liftPower, &harmBlob,
                                                       &liftBlob, &grReadout, &freq, &mode, &harmKnob, &compKnob, &liftKnob, &eqPower,
                                                       &eqCurve, &inputKnob, &outputKnob })
        content.addAndMakeVisible (*c);
    for (auto& k : eqKnobs) content.addAndMakeVisible (k);
    for (auto& b : parPower) content.addAndMakeVisible (b);
    for (auto& f : faders) content.addAndMakeVisible (f);
    content.addAndMakeVisible (stripe);
    content.addChildComponent (toast);                // over everything else (shown by Compare's arrow)
    content.addMouseListener (&hoverTracker, true);

    // top bar: the same as The Antiparticle's (ours: Compare left of "Preset" and the side-chain switch where The God
    // Particle has it, so the suite's top bars match; the reference has Compare at x 604 ... 734 and no side-chain)
    logo.setBounds (6, 7, 17, 17);
    presetBox.setBounds (176, 3, 193, 25);
    prevButton.setBounds (375, 3, 24, 25);
    nextButton.setBounds (405, 3, 24, 25);
    saveButton.setBounds (439, 3, 24, 25);
    compare.setBounds (52, 3, 74, 25);
    sidechain.setBounds (560, 3, 85, 25);
    ioLinkButton.setBounds (664, 3, 64, 25);       // centred between the side-chain switch (ends 645) and "UI Scale" (747.5)
    scaleBox.setBounds (794, 3, 66, 25);
    gear.setBounds (866, 7, 16, 17);

    // power buttons: 72 x 23.5 (parallels 38 wide) at y 41.5
    for (auto* b : { &drivePower, &harmPower, &compPower, &liftPower, &eqPower }) { b->boxHeight = 23.5f; b->offHoverFill = true; }
    place (drivePower, 96.0f, 41.5f, 72.0f, 23.5f);
    place (harmPower, 184.0f, 41.5f, 72.0f, 23.5f);
    place (compPower, 264.0f, 41.5f, 72.0f, 23.5f);
    place (liftPower, 344.0f, 41.5f, 72.0f, 23.5f);
    place (eqPower, 481.0f, 41.5f, 72.0f, 23.5f);
    for (int k = 0; k < 4; ++k)
    {
        parPower[k].boxHeight = 23.5f;
        parPower[k].offHoverFill = true;
        place (parPower[k], parX[k] - 19.0f, 41.5f, 38.0f, 23.5f);
        place (faders[k], parX[k] - sc::Fader::capW * 0.5f, sc::Fader::top, sc::Fader::capW, 180.0f);
    }

    place (sparks, 96.0f, 41.0f, 72.0f, 88.0f);
    place (hiPass, 96.0f, 133.5f, 72.0f, 23.5f);
    place (color, 95.0f, 179.5f, 73.0f, 68.5f);
    place (freq, 184.0f, 133.5f, 72.0f, 45.5f);
    place (mode, 264.0f, 133.5f, 72.0f, 45.5f);
    place (harmBlob, 220.0f - 25.0f, 99.25f - 25.0f, 50.0f, 50.0f);
    place (liftBlob, 380.0f - 25.0f, 99.25f - 25.0f, 50.0f, 50.0f);
    place (eqCurve, 432.0f, 70.0f, 168.0f, 55.0f);
    place (grReadout, 300.5f - 40.0f, 106.0f - 22.0f, 80.0f, 28.0f);      // (text baseline at y 106)
    place (toast, 344.0f, 337.0f, 200.0f, 60.5f);
    place (stripe, sc::Stripe::left, sc::Stripe::top, sc::Stripe::width, sc::Stripe::height);
    stripe.paintBackground = [this] (Graphics& g) { paintPanel (g); };

    placeKnob (harmKnob, harmX, bottomRowY, smallR);
    placeKnob (compKnob, compX, bottomRowY, smallR);
    placeKnob (liftKnob, liftX, bottomRowY, smallR);
    for (int b = 0; b < 5; ++b) placeKnob (eqKnobs[b], eqX[b], eqTop[b] ? topRowY : bottomRowY, smallR);
    placeKnob (inputKnob, inputX, plateKnobY, plateR);
    placeKnob (outputKnob, outputX, plateKnobY, plateR);

    auto& s = proc.apvts;
    auto attach = [&] (Slider& sl, const char* pid)
    {
        attachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, pid, sl));
        if (auto* prm = dynamic_cast<RangedAudioParameter*> (s.getParameter (pid)))
        {
            if (auto* mk = dynamic_cast<orrery::MetalKnob*> (&sl))
            {
                mk->setDefaultValue (prm->convertFrom0to1 (prm->getDefaultValue()));
                mk->onGesture = [prm] (bool begin) { if (begin) prm->beginChangeGesture(); else prm->endChangeGesture(); };
            }
            else sl.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        }
    };
    attach (harmKnob, sc::id::harmAmount);
    attach (compKnob, sc::id::compAmount);
    attach (liftKnob, sc::id::liftAmount);
    for (int b = 0; b < 5; ++b) attach (eqKnobs[b], sc::eqIds[b]);
    for (int k = 0; k < 4; ++k) attach (faders[k], sc::parallels[k].level);
    attach (inputKnob, sc::id::inputGain);
    attach (outputKnob, sc::id::outputGain);

    for (auto* k : { &inputKnob, &outputKnob })
    {
        k->onDragStart = [this, k] { beginLink (*k); };
        k->onValueChange = [this, k] { if (linkSource == k) mirror (k == &inputKnob); };
        k->onDragEnd = [this, k] { endLink (*k); };
    }
    ioLinkButton.active = proc.ioLink.load();
    ioLinkButton.onClick = [this]
    {
        proc.ioLink = ! proc.ioLink.load();
        ioLinkButton.active = proc.ioLink.load();
        ioLinkButton.repaint();
        if (proc.ioLink.load())                    // linking snaps the output to the input's opposite
            if (auto* prm = proc.apvts.getParameter (sc::id::outputGain))
            {
                prm->beginChangeGesture();
                mirror (true);
                prm->endChangeGesture();
            }
    };

    drivePower.onClick = [this] { toggleParam (sc::id::driveOn); };
    hiPass.onClick = [this] { toggleParam (sc::id::driveHiPass); };
    harmPower.onClick = [this] { toggleParam (sc::id::harmOn); };
    compPower.onClick = [this] { toggleParam (sc::id::compOn); };
    liftPower.onClick = [this] { toggleParam (sc::id::liftOn); };
    eqPower.onClick = [this] { toggleParam (sc::id::eqOn); };
    for (int k = 0; k < 4; ++k) parPower[k].onClick = [this, k] { toggleParam (sc::parallels[k].on); };
    auto choose = [this] (const char* pid, int index, int count)
    {
        if (auto* prm = proc.apvts.getParameter (pid))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost ((float) index / (float) (count - 1));
            prm->endChangeGesture();
        }
        updateFromParams();
    };
    color.onChange = [choose] (int i) { choose (sc::id::driveColor, i, 3); };
    mode.onChange = [choose] (int i) { choose (sc::id::compMode, i, 2); };
    freq.onChange = [choose] (int i) { choose (sc::id::harmFreq, i, 2); };
    for (auto* pid : sc::presetIds()) s.addParameterListener (pid, this);

    sidechain.external = proc.sidechainExternal.load();
    sidechain.onChange = [this] (bool ext) { proc.sidechainExternal = ext; };
    compare.slot = proc.compareSlot();
    compare.onSelect = [this] (int slot)
    {
        proc.selectCompareSlot (slot);
        compare.slot = proc.compareSlot();
        presetBox.text = proc.presets.currentName();
        compare.repaint(); presetBox.repaint();
        updateFromParams();
    };
    compare.onCopy = [this]
    {
        proc.copyCompareSlot();
        toast.show (proc.compareSlot() == 0 ? "A copied to B." : "B copied to A.");
    };
    presetBox.onClick = [this] { if (dropList != nullptr) closePopups(); else showPresetList(); };
    scaleBox.onClick = [this] { if (dropList != nullptr) closePopups(); else showScaleList(); };
    prevButton.onClick = [this] { proc.presets.step (-1); presetBox.text = proc.presets.currentName(); presetBox.repaint(); };
    nextButton.onClick = [this] { proc.presets.step (1); presetBox.text = proc.presets.currentName(); presetBox.repaint(); };
    saveButton.onClick = [this] { showSavePreset(); };
    gear.onClick = [this] { showSettings(); };
    logo.onClick = [this] { showAboutMenu(); };
    presetBox.text = proc.presets.currentName();

    updateFromParams();
    constrainer.setFixedAspectRatio (designW / designH);
    constrainer.setSizeLimits ((int) (designW * scales[0]), (int) (designH * scales[0]),
                               (int) (designW * scales[std::size (scales) - 1]), (int) (designH * scales[std::size (scales) - 1]));
    setResizable (true, false);          // hosts may resize the window too; the corner below is our own
    setConstrainer (&constrainer);
    grip = std::make_unique<Grip> (this, &constrainer);
    orrery::noFocus (*grip);
    addAndMakeVisible (*grip);
    grip->setAlwaysOnTop (true);
    setUiScale (proc.uiScale.load());
    animateChanges = true;
    startTimerHz (60);                   // (the animations' clock: the reference's stripe never repeats a frame in a film at 36
                                         // frames a second; at 40 Hz ours, busy painting, managed ~31)
    readoutClock.startTimer (46);
}

ScorpiusEditor::~ScorpiusEditor()
{
    readoutClock.stopTimer();
    content.removeMouseListener (&hoverTracker);
    for (auto* pid : sc::presetIds()) proc.apvts.removeParameterListener (pid, this);
}

void ScorpiusEditor::toggleParam (const char* pid)
{
    if (auto* prm = proc.apvts.getParameter (pid))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() >= 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
    }
    updateFromParams();         // at once (the listener's update comes a message later)
}

void ScorpiusEditor::setUiScale (float s)
{
    scale = s;
    proc.uiScale = s;
    scaleBox.text = String (roundToInt (s * 100.0f)) + "%";
    content.setTransform (AffineTransform::scale (s));
    setSize ((int) (designW * s), (int) (designH * s));      // truncated, as the reference (403 px at 100 %)
}

// a new size (the corner, the host, or setUiScale) sets the scale; sizes from setUiScale itself keep their exact scale
void ScorpiusEditor::resized()
{
    if (std::abs (getWidth() - (int) (designW * scale)) > 1)
    {
        scale = jlimit (scales[0], scales[std::size (scales) - 1], (float) getWidth() / designW);
        proc.uiScale = scale;
        scaleBox.text = String (roundToInt (scale * 100.0f)) + "%";
        scaleBox.repaint();
        content.setTransform (AffineTransform::scale (scale));
        getProperties().set ("liveResize", true);        // stretch the cached images until the size settles (timer)
        lastResizeMs = Time::getMillisecondCounter();
    }
    if (grip != nullptr)
    {
        const int g = roundToInt (18.0f * scale);
        grip->setBounds (getWidth() - g, getHeight() - g, g, g);
    }
}

void ScorpiusEditor::Grip::paint (Graphics& g)
{
    const float w = (float) getWidth(), u = w / 18.0f;
    const auto light = theme::plateText.withAlpha (isMouseOverOrDragging() ? 0.95f : 0.6f);
    for (int i = 1; i <= 3; ++i)
    {
        const float d = (float) i * 4.6f * u;
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawLine (w - d - 0.9f * u, w - 2.4f * u, w - 2.4f * u, w - d - 0.9f * u, 1.3f * u);
        g.setColour (light);
        g.drawLine (w - d, w - 2.4f * u, w - 2.4f * u, w - d, 1.3f * u);
    }
}

void ScorpiusEditor::Grip::mouseUp (const MouseEvent& e)
{
    ResizableCornerComponent::mouseUp (e);
    if (auto* ed = findParentComponentOfClass<ScorpiusEditor>()) ed->endLiveResize();
}

void ScorpiusEditor::endLiveResize()
{
    if (! getProperties().getWithDefault ("liveResize", false)) return;
    getProperties().set ("liveResize", false);
    content.repaint();
}

void ScorpiusEditor::parameterChanged (const String& pid, float)
{
    const bool gain = pid == sc::id::inputGain || pid == sc::id::outputGain, fromInput = pid == sc::id::inputGain;
    MessageManager::callAsync ([safe = Component::SafePointer<ScorpiusEditor> (this), gain, fromInput]
    {
        if (safe == nullptr) return;
        if (gain && safe->linkSource == nullptr) safe->mirror (fromInput);      // automation, presets: keep the link
        safe->updateFromParams();
    });
}

void ScorpiusEditor::beginLink (Slider& src)
{
    if (! proc.ioLink.load() || linkSource != nullptr) return;
    if (auto* prm = proc.apvts.getParameter (&src == &inputKnob ? sc::id::outputGain : sc::id::inputGain))
    {
        linkSource = &src;
        prm->beginChangeGesture();
        mirror (&src == &inputKnob);
    }
}

void ScorpiusEditor::mirror (bool fromInput)
{
    if (! proc.ioLink.load() || mirroring) return;
    const ScopedValueSetter<bool> guard (mirroring, true);
    const auto* srcId = fromInput ? sc::id::inputGain : sc::id::outputGain;
    const auto* dstId = fromInput ? sc::id::outputGain : sc::id::inputGain;
    if (auto* dst = dynamic_cast<RangedAudioParameter*> (proc.apvts.getParameter (dstId)))
    {
        const float target = dst->getNormalisableRange().snapToLegalValue (-proc.apvts.getRawParameterValue (srcId)->load());
        if (std::abs (proc.apvts.getRawParameterValue (dstId)->load() - target) > 1.0e-5f)
            dst->setValueNotifyingHost (dst->convertTo0to1 (target));
    }
    content.repaint();
}

void ScorpiusEditor::endLink (Slider& src)
{
    if (linkSource != &src) return;
    if (auto* prm = proc.apvts.getParameter (&src == &inputKnob ? sc::id::outputGain : sc::id::inputGain))
        prm->endChangeGesture();
    linkSource = nullptr;
    content.repaint();
}

void ScorpiusEditor::updateFromParams()
{
    auto v = [this] (const char* pid) { return proc.apvts.getRawParameterValue (pid)->load(); };
    auto on = [&v] (const char* pid) { return v (pid) >= 0.5f; };
    drivePower.active = on (sc::id::driveOn);
    hiPass.active = on (sc::id::driveHiPass);
    harmPower.active = on (sc::id::harmOn);
    compPower.active = on (sc::id::compOn);
    liftPower.active = on (sc::id::liftOn);
    eqPower.active = on (sc::id::eqOn);
    color.selected = jlimit (0, 2, roundToInt (v (sc::id::driveColor)));
    mode.selected = jlimit (0, 1, roundToInt (v (sc::id::compMode)));
    freq.selected = v (sc::id::harmFreq) < 65.0f ? 0 : 1;
    sparks.setHiPass (hiPass.active, animateChanges);
    sparks.setColour (color.selected, animateChanges);
    fades.show (sparks, drivePower.active, animateChanges);
    fades.show (harmBlob, harmPower.active, animateChanges);
    fades.show (grReadout, compPower.active, animateChanges);
    fades.show (liftBlob, liftPower.active, animateChanges);
    fades.show (eqCurve, eqPower.active, animateChanges);
    for (int b = 0; b < 5; ++b) eqCurve.gains[b] = v (sc::eqIds[b]);
    for (int k = 0; k < 4; ++k) { parPower[k].active = on (sc::parallels[k].on); faders[k].on = parPower[k].active; faders[k].repaint(); }
    for (auto* c : std::initializer_list<Component*> { &drivePower, &hiPass, &harmPower, &compPower, &liftPower, &eqPower, &color, &mode, &freq,
                                                       &harmBlob, &liftBlob, &eqCurve })
        c->repaint();
    for (auto& b : parPower) b.repaint();
    presetBox.text = proc.presets.currentName();
    presetBox.repaint();
    content.repaint();
}

void ScorpiusEditor::updateHover (const MouseEvent& e, bool released)
{
    const auto p = e.getEventRelativeTo (&content).position;
    const Slider* t = nullptr;
    std::vector<const Slider*> all { &harmKnob, &compKnob, &liftKnob, &inputKnob, &outputKnob };
    for (auto& k : eqKnobs) all.push_back (&k);
    for (auto& f : faders) all.push_back (&f);
    if (! released)
        for (auto* k : all)
            if (k->isMouseButtonDown()) t = k;                // a drag keeps its caption wherever the mouse goes
    const bool inside = content.getLocalBounds().toFloat().contains (p) && dialog == nullptr && dropList == nullptr;
    if (t == nullptr && inside)
        for (auto* k : all)
            if (hoverCell (*k).contains (p)) t = k;
    if (t != hovered) { hovered = t; content.repaint(); }
}

// where hovering shows a control's value (measured on the reference with hover probes): a knob of the panel over a
// cell taking in its label and some 32 px either side; a fader over its column from under its power button down to its
// label; the plate knobs over their plate column
Rectangle<float> ScorpiusEditor::hoverCell (const Slider& k) const
{
    if (&k == &inputKnob || &k == &outputKnob)
        return { (&k == &inputKnob ? inputX : outputX) - 43.5f, plateTop, 87.0f, designH - plateTop };
    if (&k == &harmKnob || &k == &compKnob || &k == &liftKnob)
    {
        const float cx = &k == &harmKnob ? harmX : &k == &compKnob ? compX : liftX;
        return { cx - 40.0f, bottomRowY - 30.0f, 80.0f, 38.0f + 30.0f };
    }
    for (int i = 0; i < 4; ++i)
        if (&k == &faders[i]) return { parX[i] - 18.5f, 66.0f, 37.0f, 270.0f - 66.0f };
    for (int b = 0; b < 5; ++b)
        if (&k == &eqKnobs[b])
        {
            const float cy = eqTop[b] ? topRowY : bottomRowY;
            return { eqX[b] - 31.0f, cy - 30.0f, 62.0f, 38.0f + 30.0f };
        }
    return {};
}

String ScorpiusEditor::caption (const Slider& k, const String& name) const
{
    const bool linkedPartner = linkSource != nullptr && linkSource != &k && (&k == &inputKnob || &k == &outputKnob);
    if (&k != hovered && ! linkedPartner) return name;
    // as the reference: amounts in percent ("80.0%"), the EQ and the gains in dB ("0.0 dB"), the faders as the gain of
    // their path in dB without a unit ("-8.0"; "-inf" at the bottom): 2 (e^(k L) - 1) / (e^k - 1), k = 2.188, fitted
    // to its captions at 14 levels to 0.03 dB
    if (&k == &harmKnob || &k == &compKnob || &k == &liftKnob) return String (k.getValue() * 100.0, 1) + "%";
    for (auto& f : faders)
        if (&k == &f)
        {
            const double lv = k.getValue();
            if (lv <= 0.0) return "-inf";
            const double gain = 1.9965 * (std::exp (2.1883 * lv) - 1.0) / (std::exp (2.1883) - 1.0);
            return String (Decibels::gainToDecibels (gain), 1);
        }
    return String::formatted ("%.1f dB", k.getValue());
}

void ScorpiusEditor::timerCallback()
{
    if (getProperties().getWithDefault ("liveResize", false) && Time::getMillisecondCounter() - lastResizeMs > 150) endLiveResize();
    if (frozen) return;
    inMeter[0].update (proc.inMeter.left);
    inMeter[1].update (proc.inMeter.right);
    outMeter[0].update (proc.outMeter.left);
    outMeter[1].update (proc.outMeter.right);
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) jlimit (0.0, 0.2, now - lastTick);
    lastTick = now;
    // the compression readout: its ballistics are the processor's (dsp/Compressor.h)
    compGrDb = proc.compGr.load();
    if (grText (compGrDb) != grReadout.text) { grReadout.text = grText (compGrDb); grReadout.repaint(); }
    const int blocks = proc.blocksProcessed.load();
    const bool running = blocks != lastBlocks;
    lastBlocks = blocks;
    // (hosts with long buffers, FL Studio's among them, deliver audio less often than this clock runs: a frame without
    // a new block is not a stop until 0.1 s have passed without one)
    idleSeconds = running ? 0.0f : idleSeconds + dt;
    const bool playing = idleSeconds < 0.1f;
    // the circles follow the lows and the mids (Visuals.h; a frame without a new spectrum keeps the last one)
    auto spectrum = [playing] (float fresh, float& last) { if (fresh > 0.0f || ! playing) last = fresh; return last; };
    harmBlob.update (spectrum (proc.visuals.takeLows(), lastLows), dt);
    liftBlob.update (spectrum (proc.visuals.takeMids(), lastMids), dt);
    stepStripe (playing, dt);
    // only the meters change on this clock (the rest repaints when it changes)
    content.repaint (0, 36, (int) columnX[0], 236);
    content.repaint ((int) columnX[4] + 1, 36, (int) (designW - columnX[4]), 236);
}

void ScorpiusEditor::sampleReadouts()
{
    if (frozen) return;
    orrery::Readouts* meters[4] { &inMeter[0], &inMeter[1], &outMeter[0], &outMeter[1] };
    orrery::LevelTap* taps[4] { &proc.inMeter.left, &proc.inMeter.right, &proc.outMeter.left, &proc.outMeter.right };
    for (int i = 0; i < 4; ++i) meters[i]->samplePeak (*taps[i]);
    if (++readoutTicks < 5) return;
    readoutTicks = 0;
    const int blocks = proc.blocksProcessed.load();
    const bool running = blocks != readoutBlocks;
    readoutBlocks = blocks;
    for (int i = 0; i < 4; ++i) meters[i]->sampleRms (*taps[i], running);
}

// the stripe's light follows the processor's level (dsp/Light.h); when the host stops calling the processor the level
// stays where it was, so the editor lets it fall as after a stop
void ScorpiusEditor::stepStripe (bool playing, float dt)
{
    if (playing) lightDb = proc.light.levelDb();
    else lightDb = jmax (-200.0f, lightDb - sc::Light::releaseDbPerSec * dt);
    const int beats = proc.light.beatCount();
    const bool beat = beats != lastBeats;
    lastBeats = beats;
    stripe.setStyle (proc.particleStyle.load() == 0 ? sc::Stripe::Style::orion : sc::Stripe::Style::scorpius);
    stripe.setBands (lastLows, lastMids);
    stripe.setTempo (proc.hostBpm.load());
    stripe.setInput (lightDb, beat, proc.animation.load());
}

void ScorpiusEditor::filmStep (double now)
{
    stripe.clock = [now] { return now; };
    if (const float l = proc.visuals.takeLows(); l > 0.0f) lastLows = l;
    if (const float m = proc.visuals.takeMids(); m > 0.0f) lastMids = m;
    stepStripe (true, 0.025f);
}

void ScorpiusEditor::setTestLevels (float inDb, float outDb, bool audio)
{
    frozen = true;
    animateChanges = false;
    updateFromParams();
    for (auto& m : inMeter) { m.barDb = inDb - 3.0f; m.fastDb = inDb; m.peakHoldDb = inDb; m.rmsDb = inDb - 1.5f; }
    for (auto& m : outMeter) { m.barDb = outDb - 3.0f; m.fastDb = outDb; m.peakHoldDb = outDb; m.rmsDb = outDb - 1.5f; }
    harmBlob.freeze (audio ? Decibels::decibelsToGain (inDb - 3.0f) : 0.0f);
    liftBlob.freeze (audio ? Decibels::decibelsToGain (inDb - 8.0f) : 0.0f);
    stripe.setStyle (proc.particleStyle.load() == 0 ? sc::Stripe::Style::orion : sc::Stripe::Style::scorpius);
    if (audio && proc.animation.load()) stripe.freeze (sc::Stripe::brightness (inDb));
    content.repaint();
}

//==============================================================================
void ScorpiusEditor::Content::paint (Graphics& g)
{
    g.fillAll (Colours::black);
    // (only what a repaint touches: the meters repaint 40 times a second)
    if (g.clipRegionIntersects ({ 0, 0, (int) designW, 32 })) ed.paintTopBar (g);
    if (g.clipRegionIntersects ({ 0, 30, (int) designW, 282 })) ed.paintPanel (g);
    if (g.clipRegionIntersects ({ 0, 306, (int) designW, (int) designH - 305 })) ed.paintPlate (g);
}

void ScorpiusEditor::Content::mouseDown (const MouseEvent& e)
{
    // clicking a readout resets the held peak of that meter (the RMS readout is live)
    const auto p = e.position;
    if (p.y > 40.0f && p.y < 84.0f)
    {
        if (p.x < columnX[0]) for (auto& m : ed.inMeter) m.peakHoldDb = -200.0f;
        if (p.x > columnX[4]) for (auto& m : ed.outMeter) m.peakHoldDb = -200.0f;
        repaint();
    }
    if (ed.dropList != nullptr) ed.closePopups();
}

void ScorpiusEditor::paintTopBar (Graphics& g)
{
    g.setColour (theme::topBar);
    g.fillRect (0.0f, 0.0f, designW, 30.5f);
    g.setColour (theme::text);
    g.setFont (theme::font (12.0f));
    theme::textAt (g, "Preset", 136.0f, 19.5f);
    theme::textAt (g, "Sidechain", 500.5f, 19.5f);
    theme::textAt (g, "UI Scale", 747.5f, 19.5f);
}

void ScorpiusEditor::paintPanel (Graphics& g)
{
    const Rectangle<float> panel (1.5f, panelTop, designW - 3.0f, panelBottom - panelTop);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (panel, 15.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (panel, 15.0f, 2.2f);
    {
        // thin glass edge inside the outline, visible on the left
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (Rectangle<int> (0, 45, 9, 252));
        g.setColour (Colour (0xff8a8a8a));
        g.drawRoundedRectangle (panel.reduced (4.0f), 11.0f, 1.0f);
    }
    g.setColour (theme::divider);
    for (float x : columnX) g.fillRect (x, dividerTop, 1.0f, dividerBottom - dividerTop);
    g.fillRect (4.5f, stripeLine, designW - 9.0f, 1.0f);

    paintIoMeter (g, false);
    paintIoMeter (g, true);

    // labels: under the knobs and selectors, mint whether their section runs or not (as the reference)
    auto label = [&g] (const String& t, float cx, float baseline, bool)
    {
        g.setColour (theme::mint);
        theme::textAt (g, t, cx, baseline, Justification::horizontallyCentred);
    };
    g.setFont (theme::font (12.0f));
    label ("color", 131.75f, labelBaseline, drivePower.active);
    label ("freq", 220.0f, selectorLabelBaseline, harmPower.active);
    label ("mode", 300.0f, selectorLabelBaseline, compPower.active);
    label (caption (harmKnob, "harmonics"), 220.25f, labelBaseline, harmPower.active);
    label (caption (compKnob, "compression"), 299.75f, labelBaseline, compPower.active);
    label (caption (liftKnob, "lift"), 380.25f, labelBaseline, liftPower.active);
    const char* eqNames[5] { "40Hz", "100Hz", "1kHz", "3kHz", "10kHz" };
    for (int b = 0; b < 5; ++b)
        label (caption (eqKnobs[b], eqNames[b]), eqLabelX[b], eqTop[b] ? upperLabelBaseline : labelBaseline, eqPower.active);
    for (int k = 0; k < 4; ++k) label (caption (faders[k], sc::parallels[k].name), parX[k], labelBaseline, parPower[k].active);

    paintStripe (g);
}

void ScorpiusEditor::paintIoMeter (Graphics& g, bool output)
{
    const auto& m = output ? outMeter : inMeter;
    const float (&cx)[2] = readoutCentres[output ? 1 : 0];
    g.setFont (theme::font (12.0f));
    for (int c = 0; c < 2; ++c)
    {
        g.setColour (m[c].peakHoldDb > 0.0f ? theme::clip : theme::mint);
        g.drawText (orrery::dbText (m[c].peakHoldDb), Rectangle<float> (cx[c] - 20.0f, 46.0f, 40.0f, 16.0f), Justification::centredTop, false);
        g.setColour (theme::mint);
        g.drawText (orrery::dbText (m[c].rmsDb), Rectangle<float> (cx[c] - 20.0f, 66.0f, 40.0f, 16.0f), Justification::centredTop, false);
    }
    const float (&lx)[2] = ledX[output ? 1 : 0];
    for (int c = 0; c < 2; ++c)
    {
        // green up to the RMS, mint from there up to the fast peak, and one mint LED at the held peak
        auto topLed = [] (float db) { int top = -1; for (int k = 0; k < theme::numLeds; ++k) if (db > theme::ledThresholdDb (k)) top = k; return top; };
        const int top = topLed (m[c].barDb), run = topLed (m[c].fastDb);
        const int dot = m[c].peakHoldDb > 0.0f ? theme::numLeds : topLed (m[c].peakHoldDb);
        g.setColour (theme::ledGreen);
        for (int k = 0; k <= top; ++k) g.fillRect (lx[c], ledBottom - 4.0f * (float) k, 8.0f, 1.0f);
        g.setColour (theme::mint);
        for (int k = top + 1; k <= run; ++k) g.fillRect (lx[c], ledBottom - 4.0f * (float) k, 8.0f, 1.0f);
        if (dot > jmax (top, run) && dot < theme::numLeds) g.fillRect (lx[c], ledBottom - 4.0f * (float) dot, 8.0f, 1.0f);
    }
    if (output) return;
    // the input target (as The God Particle's, narrower and higher): fills with the fast peak from -12 to -3 dBFS
    const float fast = jmax (m[0].fastDb, m[1].fastDb);
    const float fill = jlimit (0.0f, 1.0f, (fast + 13.5f) / 1.5f) * jlimit (0.0f, 1.0f, (-1.5f - fast) / 1.5f);
    static constexpr float top = 101.0f, bottom = 119.5f, r = 2.5f;
    auto bracket = [&g, fill] (float x, bool right)
    {
        if (fill > 0.0f)
        {
            Path f;
            f.addRoundedRectangle (x, top - 0.5f, 8.5f, bottom - top + 1.0f, r, r, ! right, right, ! right, right);
            g.setColour (theme::targetFill.withAlpha (fill));
            g.fillPath (f);
        }
        const float ox = right ? x : x + 8.5f, ix = right ? x + 8.0f : x + 0.5f, d = right ? -r : r;
        Path p;
        p.startNewSubPath (ox, top);
        p.lineTo (ix + d, top);
        p.quadraticTo (ix, top, ix, top + r);
        p.lineTo (ix, bottom - r);
        p.quadraticTo (ix, bottom, ix + d, bottom);
        p.lineTo (ox, bottom);
        g.setColour (theme::target);
        g.strokePath (p, PathStrokeType (1.0f));
    };
    bracket (15.0f, false);
    bracket (66.5f, true);
    g.setColour (theme::targetLine);
    g.fillRect (36.0f, top - 0.5f, 18.0f, 1.0f);
    g.fillRect (36.0f, bottom - 0.5f, 18.0f, 1.0f);
    if (fill > 0.0f)
    {
        g.setColour (theme::targetFill.withAlpha (fill));
        g.fillRect (36.0f, top - 0.5f, 18.0f, bottom - top + 1.0f);
    }
}

// the stripe across the panel's foot: a dark glass tube; particles stream through it while audio plays
void ScorpiusEditor::paintStripe (Graphics& g)
{
    // the dark glass tube the stripe's light plays in (measured on the reference at 200 % with the dark levels lifted):
    // a faint capsule outline, a flat grey floor along its lower half (rounded with its ends), a short cyan glint inside
    // its left end and a soft highlight inside its right end; under it the panel's inner edge as a dim band
    const Rectangle<float> tube (11.75f, 283.75f, 874.5f - 11.75f, 14.5f);
    constexpr float radius = 4.0f;
    {
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addRoundedRectangle (tube, radius);
        g.reduceClipRegion (clip);
        // floor: grey 27, turning a little green towards the right; a brighter line along its top, a soft foot
        g.setGradientFill (ColourGradient (Colour (0xff1b1b1b), tube.getX(), 0.0f, Colour (0xff1d1f1e), tube.getRight(), 0.0f, false));
        g.fillRect (tube.getX(), 290.75f, tube.getWidth(), 6.25f);
        g.setColour (Colour (0xff1f1f1f));
        g.fillRect (tube.getX(), 290.75f, tube.getWidth(), 0.5f);
        g.setGradientFill (ColourGradient (Colour (0xff151515), 0.0f, 297.0f, Colour (0x00050505), 0.0f, 298.25f, false));
        g.fillRect (tube.getX(), 297.0f, tube.getWidth(), 1.25f);
        // the right end's highlight: a tall soft patch, brightest a little left of the end
        g.setGradientFill (ColourGradient (Colour (0x10ffffff), 868.75f, 0.0f, Colour (0x96ffffff), 871.5f, 0.0f, false));
        g.fillRoundedRectangle (868.75f, 284.75f, 4.25f, 5.75f, 1.5f);
    }
    g.setColour (Colour (0xff141414));
    g.drawRoundedRectangle (tube.reduced (0.25f), radius, 0.5f);
    g.setColour (Colour (0xff9a9a9a).withAlpha (0.7f));
    g.fillRect (873.75f, 285.0f, 0.75f, 6.0f);                        // (the right end's edge, lit)
    g.setGradientFill (ColourGradient (Colour (0xff363f42), 13.25f, 0.0f, Colour (0x00363f42), 19.25f, 0.0f, false));
    g.fillRoundedRectangle (13.0f, 285.25f, 6.25f, 1.25f, 0.6f);      // the left end's glint
    {
        // the panel's inner edge along the bottom and up the corners, a dim band (grey ~36)
        Graphics::ScopedSaveState s (g);
        const auto inner = Rectangle<float> (1.5f, panelTop, designW - 3.0f, panelBottom - panelTop).reduced (4.25f);
        g.reduceClipRegion (Rectangle<int> (0, 291, (int) designW, 20));
        g.setColour (Colour (0xff242424));
        g.drawRoundedRectangle (inner, 10.75f, 2.5f);
        // (round the bottom-left corner the bright edge of the left side fades into it)
        g.setGradientFill (ColourGradient (Colour (0xff8a8a8a), 5.0f, 296.0f, Colour (0xff242424), 16.0f, 305.0f, false));
        g.reduceClipRegion (Rectangle<int> (0, 291, 16, 20));
        g.drawRoundedRectangle (inner, 10.75f, 1.5f);
    }
    // (the light that plays in it is the Stripe component's)
}

void ScorpiusEditor::paintPlate (Graphics& g)
{
    // the plate (as The God Particle's: a separate rounded panel with a leather grain, a short shadow at the top)
    g.setColour (Colours::black);
    g.fillRect (0.0f, panelBottom + 1.5f, designW, designH - panelBottom - 1.5f);
    const Rectangle<float> plate (0.5f, plateTop, designW - 1.0f, designH - plateTop);
    Path platePath;
    platePath.addRoundedRectangle (plate, 11.0f);
    {
        const float dpx = theme::pixelScale (g, content);
        if (plateImage.isNull() || (std::abs (dpx - plateScale) > 0.001f && ! theme::liveResizing (content)))
        {
            const int w = (int) std::ceil (plate.getWidth() * dpx), h = (int) std::ceil (plate.getHeight() * dpx);
            plateImage = Image (Image::RGB, w, h, false, SoftwareImageType());
            Image::BitmapData bd (plateImage, Image::BitmapData::writeOnly);
            Random rng (7);
            std::vector<float> noise ((size_t) (w * h));
            for (auto& n : noise) n = rng.nextFloat() + rng.nextFloat() + rng.nextFloat() - 1.5f;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const float n0 = noise[(size_t) (y * w + x)];
                    const float nl = x > 0 ? noise[(size_t) (y * w + x - 1)] : 0.0f, nu = y > 0 ? noise[(size_t) ((y - 1) * w + x)] : 0.0f;
                    const float v = 7.8f * (n0 - 0.5f * (nl + nu));
                    bd.setPixelColour (x, y, Colour ((uint8) jlimit (0.0f, 255.0f, 70.0f + v), (uint8) jlimit (0.0f, 255.0f, 69.0f + v),
                                                     (uint8) jlimit (0.0f, 255.0f, 64.0f + v)));
                }
            plateScale = dpx;
        }
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (platePath);
        g.drawImage (plateImage, plate);
        g.setGradientFill (ColourGradient (Colour (0xff1e1d1b), 0.0f, plateTop, Colour (0x00464540), 0.0f, plateTop + 9.5f, false));
        g.fillRect (plate.withHeight (10.0f));
    }
    // the plate knobs' shadows (the same knobs and light as The God Particle's: shared/ui/KnobShadow.h)
    {
        const float dpx = theme::pixelScale (g, content);
        constexpr float reach = 89.0f, half = plateR + reach;
        if (knobShadow.isNull() || (std::abs (dpx - knobShadowScale) > 0.001f && ! theme::liveResizing (content)))
        {
            const int px = (int) std::ceil (2.0f * half * dpx);
            knobShadow = Image (Image::ARGB, px, px, true, SoftwareImageType());
            Image::BitmapData bd (knobShadow, Image::BitmapData::writeOnly);
            for (int y = 0; y < px; ++y)
                for (int x = 0; x < px; ++x)
                {
                    const float dx = ((float) x + 0.5f) / dpx - half, dy = ((float) y + 0.5f) / dpx - half;
                    const float d = std::sqrt (dx * dx + dy * dy) - plateR;
                    if (d < -1.0f || d >= reach) continue;
                    const float a = std::atan2 (dx, -dy);
                    const float fade = jlimit (0.0f, 1.0f, (reach - d) / 15.0f);
                    const float f = 1.0f - (1.0f - orrery::knobshadow::factor (jmax (0.0f, d), a)) * fade;
                    bd.setPixelColour (x, y, Colours::black.withAlpha (jlimit (0.0f, 1.0f, 1.0f - f)));
                }
            knobShadowScale = dpx;
        }
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (platePath);
        for (float cx : { inputX, outputX })
            g.drawImage (knobShadow, Rectangle<float> (2.0f * half, 2.0f * half).withCentre ({ cx, plateKnobY }));
    }

    // labels
    g.setColour (theme::plateText);
    g.setFont (theme::font (12.0f));
    const std::pair<String, float> labels[] { { caption (inputKnob, "input"), 46.0f }, { "drive", 128.5f }, { "harmonic eq", 516.5f },
                                              { "parallels", 705.0f }, { caption (outputKnob, "output"), 841.75f } };
    for (auto& [t, x] : labels) theme::textAt (g, t, x, 324.5f, Justification::horizontallyCentred);

    // the grille and the lettering (static: drawn once per scale into an image; they are ~400 shaded dots)
    {
        const Rectangle<float> area (170.0f, 306.0f, 252.0f, designH - 306.0f);
        if (g.clipRegionIntersects (area.getSmallestIntegerContainer()))
        {
            const float dpx = theme::pixelScale (g, content);
            if (grilleImage.isNull() || (std::abs (dpx - grilleScale) > 0.001f && ! theme::liveResizing (content)))
            {
                grilleImage = Image (Image::ARGB, (int) std::ceil (area.getWidth() * dpx), (int) std::ceil (area.getHeight() * dpx), true);
                Graphics gg (grilleImage);
                gg.addTransform (AffineTransform::translation (-area.getX(), -area.getY()).scaled (dpx));
                paintGrille (gg);
                grilleScale = dpx;
            }
            g.drawImage (grilleImage, area);
        }
    }
}

void ScorpiusEditor::paintGrille (Graphics& g)
{
    // grille: 19 staggered rows of embossed dots (21 / 20 per row; rows y 313.5 + 4.278 k, pitch 12.07 -> 11.34, x from
    // 179.3 -> 186.5), a halftone of an arch: the dots are largest along a circle about (300, 587.8) of radius 239.3
    // (area 0.043 + 0.932 exp (-((d - 239.3) / 26.3)^2) of the largest, fitted to every dot of the reference)
    for (int r = 0; r < 19; ++r)
    {
        const float t = (float) r / 18.0f;
        const float y = 313.5f + 4.278f * (float) r;
        const bool even = (r % 2) == 0;
        const float pitch = 12.07f - 0.73f * t;
        const float x0 = (even ? 179.3f : 185.7f) + 7.2f * t;
        const int count = even ? 21 : 20;
        for (int k = 0; k < count; ++k)
        {
            const Point<float> c (x0 + pitch * (float) k, y);
            const float d = c.getDistanceFrom ({ 300.0f, 587.8f });
            const float area = 0.043f + 0.932f * std::exp (-std::pow ((d - 239.3f) / 26.3f, 2.0f));
            const float rad = 2.15f * std::sqrt (area);
            g.setColour (Colour (0xff1c1b18).withAlpha (0.6f));                   // shadow below right
            g.fillEllipse (Rectangle<float> (2.0f * rad + 0.6f, 2.0f * rad + 0.6f).withCentre (c.translated (0.35f, 0.6f)));
            // a polished sphere: a small hot highlight up and to the left, falling off to a dark edge
            ColourGradient ball (Colour (0xffffffff), c.x - 0.3f * rad, c.y - 0.4f * rad, Colour (0xff2c2b29), c.x + 1.1f * rad, c.y + 1.1f * rad, true);
            ball.addColour (0.18, Colour (0xffe6e6e4));
            ball.addColour (0.5, Colour (0xff8e8e8b));
            g.setGradientFill (ball);
            g.fillEllipse (Rectangle<float> (2.0f * rad, 2.0f * rad).withCentre (c));
        }
    }

    // lettering, spaced across the grille like the reference's five letters
    const String word = "SCORPIUS";
    g.setFont (theme::font (7.75f));
    for (int i = 0; i < word.length(); ++i)
    {
        const String ch = String::charToString (word[i]);
        const float cx = 188.0f + (412.25f - 188.0f) * (float) i / (float) (word.length() - 1);
        g.setColour (Colour (0xff161513));
        theme::textAt (g, ch, cx, 401.3f, Justification::horizontallyCentred);
        g.setColour (Colour (0xffd8d8d6));
        theme::textAt (g, ch, cx, 400.5f, Justification::horizontallyCentred);
    }
}

//==============================================================================
void ScorpiusEditor::closePopups()
{
    if (dropList != nullptr) content.removeChildComponent (dropList.get());
    dropList.reset();
    presetBox.open = scaleBox.open = false;
    presetBox.repaint(); scaleBox.repaint();
}

void ScorpiusEditor::showPresetList()
{
    closePopups();
    const auto all = proc.presets.list();
    std::vector<orrery::DropList::Item> items { { "factory", true } };
    std::vector<int> map { -1 };
    for (int i = 0; i < (int) all.size(); ++i)
        if (all[(size_t) i].factory) { items.push_back ({ all[(size_t) i].name, false, all[(size_t) i].name == proc.presets.currentName() }); map.push_back (i); }
    items.push_back ({ "user", true }); map.push_back (-1);
    bool anyUser = false;
    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(size_t) i].factory) { items.push_back ({ all[(size_t) i].name }); map.push_back (i); anyUser = true; }
    if (! anyUser) { items.push_back ({ "-" }); map.push_back (-1); }
    dropList = std::make_unique<orrery::DropList>();
    dropList->setItems (items, 24.0f, true);
    dropList->onPick = [this, all, map] (int row)
    {
        const int idx = map[(size_t) row];
        if (idx >= 0) { proc.presets.load (all[(size_t) idx]); presetBox.text = proc.presets.currentName(); }
        closePopups();
    };
    dropList->setBounds (176, 27, 193, (int) (24.0f * (float) items.size()) - 2);
    presetBox.open = true;
    content.addAndMakeVisible (*dropList);
}

void ScorpiusEditor::showScaleList()
{
    closePopups();
    std::vector<orrery::DropList::Item> items;
    for (float s : scales) items.push_back ({ String (roundToInt (s * 100.0f)) + "%", false, std::abs (s - scale) < 0.01f });
    dropList = std::make_unique<orrery::DropList>();
    dropList->setItems (items, 24.0f, false);
    dropList->onPick = [safe = Component::SafePointer<ScorpiusEditor> (this)] (int row)
    {
        if (safe == nullptr) return;
        const float s = scales[row];
        safe->closePopups();
        MessageManager::callAsync ([safe, s] { if (safe != nullptr) safe->setUiScale (s); });
    };
    dropList->setBounds (794, 27, 66, (int) (24.0f * (float) items.size()) + 1);
    scaleBox.open = true;
    content.addAndMakeVisible (*dropList);
}

namespace
{
// settings page: the particle animation, on or off (the reference's only setting; layout measured at 100 %)
class SettingsDialog : public orrery::Dialog
{
public:
    // the reference's one row (particle animation) and ours (particle style: the reference's or Scorpius's own)
    explicit SettingsDialog (ScorpiusProcessor& p) : Dialog ("settings", { 234.0f, 73.0f, 420.0f, 254.0f }, false), proc (p) {}
    void paint (Graphics& g) override
    {
        Dialog::paint (g);
        g.setColour (theme::text);
        g.setFont (theme::font (16.75f));
        theme::textAt (g, "particle animation", 258.5f, 140.0f);
        theme::textAt (g, "particle style", 258.5f, 191.0f);
        segments (g, animationRow, "ON", "OFF", proc.animation.load());
        segments (g, styleRow, "ORION", "SCORPIUS", proc.particleStyle.load() == 0);
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (animationRow.contains (e.position)) { proc.animation = e.position.x < animationRow.getCentreX(); repaint(); return; }
        if (styleRow.contains (e.position)) { proc.particleStyle = e.position.x < styleRow.getCentreX() ? 0 : 1; repaint(); return; }
        Dialog::mouseDown (e);
    }
private:
    // two segments, the chosen one white (as the reference's ON / OFF)
    static void segments (Graphics& g, Rectangle<float> r, const char* left, const char* right, bool leftOn)
    {
        const float w = r.getWidth() * 0.5f;
        g.setColour (theme::boxFill);
        g.fillRoundedRectangle (r, 4.0f);
        {
            Graphics::ScopedSaveState s (g);
            Path clip; clip.addRoundedRectangle (r, 4.0f);
            g.reduceClipRegion (clip);
            g.setColour (theme::text);
            g.fillRect (leftOn ? r.withWidth (w) : r.withTrimmedLeft (w));
        }
        g.setColour (theme::text);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setFont (theme::font (12.0f));
        g.setColour (leftOn ? theme::boxFill : theme::text);
        g.drawText (left, r.withWidth (w), Justification::centred, false);
        g.setColour (leftOn ? theme::text : theme::boxFill);
        g.drawText (right, r.withTrimmedLeft (w), Justification::centred, false);
    }
    const Rectangle<float> animationRow { 441.0f, 124.0f, 182.0f, 23.0f }, styleRow { 441.0f, 175.0f, 182.0f, 23.0f };
    ScorpiusProcessor& proc;
};
}

void ScorpiusEditor::showDialog (std::unique_ptr<Component> d)
{
    closePopups();
    if (dialog != nullptr) { content.removeChildComponent (dialog.get()); dialog.reset(); }
    d->setBounds (0, 0, (int) std::ceil (designW), (int) std::ceil (designH));
    if (auto* dl = dynamic_cast<orrery::Dialog*> (d.get()))
        dl->onClose = [safe = Component::SafePointer<ScorpiusEditor> (this)]
        {
            MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->content.removeChildComponent (safe->dialog.get()); safe->dialog.reset(); } });
        };
    content.addAndMakeVisible (*d);
    dialog = std::move (d);
}

void ScorpiusEditor::showSettings() { showDialog (std::make_unique<SettingsDialog> (proc)); }

void ScorpiusEditor::showSavePreset()
{
    auto d = std::make_unique<orrery::SaveDialog> (proc.presets.currentName(), [this] (const String& n) { return proc.presets.save (n); },
                                                   [this] { presetBox.text = proc.presets.currentName(); presetBox.repaint(); });
    d->moveBy ((designH - 359.5f) * 0.5f);         // centred in this taller window
    auto* raw = d.get();
    showDialog (std::move (d));
    raw->focusName();
}

// the logo's menu (as The Antiparticle's): version, About, repository, issues, licence
void ScorpiusEditor::showAboutMenu()
{
    if (dropList != nullptr) { closePopups(); return; }
    const String version = JucePlugin_VersionString;
    dropList = std::make_unique<orrery::DropList>();
    dropList->setItems ({ { "Scorpius v" + version, true }, { "About Scorpius" }, { "Orrery Suite on GitHub" },
                          { "Report an Issue" }, { "License (AGPL-3.0)" } }, 24.0f, true);
    dropList->onPick = [safe = Component::SafePointer<ScorpiusEditor> (this), version] (int row)
    {
        if (safe == nullptr) return;
        safe->closePopups();
        if (row == 1)
        {
            const auto wrapper = safe->proc.wrapperType;
            const String format = AudioProcessor::getWrapperTypeDescription (wrapper);
            auto about = std::make_unique<orrery::InfoDialog> ("about Scorpius", std::vector<std::pair<String, String>> {
                { "Version", version + (wrapper == AudioProcessor::wrapperType_Undefined ? String() : " (" + format + ")") },
                { {}, {} },
                { "What it is", "An open-source drum-bus processor modelled on and measured against Cradle's Orion" },
                { "Part of", "Orrery Suite: open-source recreations, null-tested against the originals" },
                { "License", "Free software under the GNU Affero General Public License v3" },
                { "Built with", "JUCE, the VST3 SDK and CLAP; Instrument Sans (SIL OFL). Their licenses come with it" },
                { "Source", repository.fromFirstOccurrenceOf ("https://", false, false) },
                { {}, {} },
                { "Note", "An independent project, not affiliated with Cradle. Orion is a trademark of its owner." } });
            safe->showDialog (std::move (about));
        }
        else if (row == 2) URL (repository).launchInDefaultBrowser();
        else if (row == 3) URL (repository + "/issues/new/choose").launchInDefaultBrowser();
        else if (row == 4) URL (repository + "/blob/main/LICENSE").launchInDefaultBrowser();
    };
    dropList->setBounds (4, 27, 214, 24 * 5 - 2);
    content.addAndMakeVisible (*dropList);
}
