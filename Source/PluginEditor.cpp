#include "PluginEditor.h"
#include "Settings.h"
#include "ui/KnobShadow.h"

using namespace juce;
namespace theme = ap::theme;
static const juce::String repository = "https://github.com/MaymiPR/TheAntiparticle";

namespace
{
// Bottom plate grille, measured on the reference at 200 % (RESEARCH.md): 19 staggered rows of embossed bars on a
// 24 px pitch; per row the bar top (design px), length and highlight height shrink towards the bottom.
struct GrilleRow { float y, length, height; };
const GrilleRow grille[] {
    { 269.0f, 21.0f, 1.5f }, { 272.5f, 21.0f, 2.5f }, { 276.5f, 20.5f, 2.5f }, { 281.0f, 19.5f, 3.0f }, { 285.5f, 19.0f, 2.5f },
    { 289.5f, 18.0f, 3.0f }, { 294.0f, 17.5f, 2.5f }, { 298.5f, 16.5f, 2.0f }, { 303.0f, 15.5f, 2.0f }, { 307.0f, 15.0f, 2.0f },
    { 311.5f, 14.0f, 2.0f }, { 316.0f, 13.0f, 1.5f }, { 320.5f, 12.5f, 1.5f }, { 324.5f, 11.5f, 1.5f }, { 329.0f, 10.5f, 1.0f },
    { 333.5f, 10.0f, 1.0f }, { 338.0f, 9.0f, 1.0f }, { 342.0f, 8.0f, 1.0f }, { 346.5f, 7.0f, 1.0f } };

constexpr float columnX[] { 89.0f, 176.0f, 559.0f, 711.0f, 798.0f };
constexpr float panelTop = 31.75f, panelBottom = 263.75f;
constexpr float ledX[2][2] { { 25.0f, 61.0f }, { 821.0f, 857.0f } };   // input L R, output L R

String grText (float gr)
{
    const float v = -gr;
    return String (std::abs (v) < 0.05f ? 0.0f : v, 1);
}

String dbText (float db)
{
    if (db <= -100.0f) return "-inf";
    return String (db, 1);
}

float toDb (float lin) { return lin > 1.0e-6f ? Decibels::gainToDecibels (lin) : -200.0f; }
}

AntiparticleEditor::AntiparticleEditor (AntiparticleProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    ap::noFocus (*this);
    ap::noFocus (content);
    setOpaque (true);
    addAndMakeVisible (content);
    content.setBounds (0, 0, (int) std::ceil (designW), (int) std::ceil (designH));

    for (auto* c : std::initializer_list<Component*> { &logo, &presetBox, &prevButton, &nextButton, &saveButton, &sidechain, &scaleBox, &gear,
                                                       &visualizer, &amountText, &eqPower, &eqHigh, &eqMid, &eqLow, &amount, &amountPower, &limiterPower,
                                                       &inputKnob, &limiterKnob, &outputKnob })
        content.addAndMakeVisible (*c);

    logo.setBounds (8, 7, 14, 17);
    presetBox.setBounds (176, 3, 193, 25);
    prevButton.setBounds (375, 3, 24, 25);
    nextButton.setBounds (405, 3, 24, 25);
    saveButton.setBounds (439, 3, 24, 25);
    sidechain.setBounds (560, 3, 85, 25);
    scaleBox.setBounds (794, 3, 66, 25);
    gear.setBounds (866, 7, 16, 17);
    visualizer.setBounds (177, 33, 382, 230);
    amountText.setBounds (360, 100, 140, 64);
    amountText.setInterceptsMouseClicks (false, false);
    amountText.paintFn = [this] (Graphics& g) { g.addTransform (AffineTransform::translation (-360.0f, -100.0f)); paintAmountText (g); };

    auto knobAt = [] (Component& k, float cx, float cy, float r) { k.setBounds (Rectangle<float> (2 * r, 2 * r).withCentre ({ cx, cy }).getSmallestIntegerContainer()); };
    // fractional centres: the knobs are placed with a transform so they land on the measured centres
    auto place = [] (Component& k, float cx, float cy, float r)
    {
        const int s = (int) std::ceil (2.0f * r);
        k.setBounds (0, 0, s, s);
        k.setTransform (AffineTransform::scale (2.0f * r / (float) s).translated (cx - r, cy - r));
    };
    ignoreUnused (knobAt);
    place (eqHigh, 133.375f, 90.75f, 20.375f);
    place (eqMid, 133.375f, 155.75f, 20.375f);
    place (eqLow, 133.375f, 220.75f, 20.375f);
    place (amount, 292.875f, 148.25f, 50.875f);
    place (inputKnob, 45.755f, 313.11f, 25.06f);
    place (limiterKnob, 754.745f, 313.11f, 25.06f);
    place (outputKnob, 841.74f, 313.11f, 25.06f);
    eqPower.setBounds (97, 40, 72, 25);
    amountPower.setBounds (364, 167, 72, 25);
    limiterPower.setBounds (719, 40, 72, 25);

    auto& s = proc.apvts;
    auto attach = [&] (ap::MetalKnob& sl, const char* pid)
    {
        attachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, pid, sl));
        if (auto* prm = dynamic_cast<RangedAudioParameter*> (s.getParameter (pid)))
            sl.setDefaultValue (prm->convertFrom0to1 (prm->getDefaultValue()));
    };
    attach (eqHigh, ap::id::eqHigh);
    attach (eqMid, ap::id::eqMid);
    attach (eqLow, ap::id::eqLow);
    attach (amount, ap::id::character);
    attach (inputKnob, ap::id::inputGain);
    attach (limiterKnob, ap::id::limiterGain);
    attach (outputKnob, ap::id::outputGain);

    auto toggleParam = [&s] (const char* pid)
    {
        if (auto* prm = s.getParameter (pid))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getValue() >= 0.5f ? 0.0f : 1.0f);
            prm->endChangeGesture();
        }
    };
    eqPower.onClick = [toggleParam] { toggleParam (ap::id::eqBypass); };
    amountPower.onClick = [toggleParam] { toggleParam (ap::id::characterBypass); };
    limiterPower.onClick = [toggleParam] { toggleParam (ap::id::limiterBypass); };
    for (auto* pid : { ap::id::eqBypass, ap::id::characterBypass, ap::id::limiterBypass, ap::id::character })
        s.addParameterListener (pid, this);

    sidechain.external = proc.sidechainExternal.load();
    sidechain.onChange = [this] (bool ext) { proc.sidechainExternal = ext; };
    presetBox.onClick = [this] { if (dropList != nullptr) closePopups(); else showPresetList(); };
    scaleBox.onClick = [this] { if (dropList != nullptr) closePopups(); else showScaleList(); };
    prevButton.onClick = [this] { proc.presets.step (-1); presetBox.text = proc.presets.currentName(); presetBox.repaint(); };
    nextButton.onClick = [this] { proc.presets.step (1); presetBox.text = proc.presets.currentName(); presetBox.repaint(); };
    saveButton.onClick = [this] { showSavePreset(); };
    gear.onClick = [this] { showSettings(); };
    logo.onClick = [this] { showAboutMenu(); };
    presetBox.text = proc.presets.currentName();

    ap::Settings::get().addChangeListener (this);
    updateFromParams();
    setUiScale (proc.uiScale.load());
    startTimerHz (30);
}

AntiparticleEditor::~AntiparticleEditor()
{
    ap::Settings::get().removeChangeListener (this);
    for (auto* pid : { ap::id::eqBypass, ap::id::characterBypass, ap::id::limiterBypass, ap::id::character })
        proc.apvts.removeParameterListener (pid, this);
}

void AntiparticleEditor::setUiScale (float s)
{
    scale = s;
    proc.uiScale = s;
    scaleBox.text = String (roundToInt (s * 100.0f)) + "%";
    content.setTransform (AffineTransform::scale (s));
    setSize ((int) (designW * s), (int) (designH * s));      // truncated, as the reference (359 px at 100 %)
}

void AntiparticleEditor::resized() {}

void AntiparticleEditor::parameterChanged (const String&, float)
{
    MessageManager::callAsync ([safe = Component::SafePointer<AntiparticleEditor> (this)] { if (safe) safe->updateFromParams(); });
}

void AntiparticleEditor::updateFromParams()
{
    auto v = [this] (const char* pid) { return proc.apvts.getRawParameterValue (pid)->load(); };
    eqOn = v (ap::id::eqBypass) < 0.5f;
    characterOn = v (ap::id::characterBypass) < 0.5f;
    limiterOn = v (ap::id::limiterBypass) < 0.5f;
    characterValue = v (ap::id::character);
    eqPower.active = eqOn;
    amountPower.active = characterOn;
    limiterPower.active = limiterOn;
    eqPower.repaint(); amountPower.repaint(); limiterPower.repaint();
    content.repaint();
}

void AntiparticleEditor::changeListenerCallback (ChangeBroadcaster*) { content.repaint(); }

void AntiparticleEditor::updateMeter (Meter& m, ap::LevelTap& tap, float dt)
{
    const float pk = toDb (tap.takePeak());
    const float rms = toDb (tap.rms.load());
    m.framePeakDb = toDb (tap.follower.load());
    m.rawPeakDb = pk;
    m.peakHoldDb = jmax (m.peakHoldDb, pk);
    m.rmsHoldDb = jmax (m.rmsHoldDb, rms);
    m.barDb = rms;
    if (pk >= m.peakDb) { m.peakDb = pk; m.peakAge = 0.0; }
    else
    {
        m.peakAge += dt;
        if (m.peakAge > 1.0) m.peakDb = jmax (pk, m.peakDb - 20.0f * dt);   // placeholder ballistics (Phase 3)
    }
}

void AntiparticleEditor::timerCallback()
{
    if (frozen) return;
    const float dt = 1.0f / 30.0f;
    updateMeter (inMeter[0], proc.inL, dt);
    updateMeter (inMeter[1], proc.inR, dt);
    updateMeter (outMeter[0], proc.outL, dt);
    updateMeter (outMeter[1], proc.outR, dt);
    for (int b = 0; b < 3; ++b) { bandGrDb[b] = proc.bandGr[b].exchange (0.0f); bandGrHold[b] = jmax (bandGrHold[b], bandGrDb[b]); }
    for (int c = 0; c < 2; ++c) { limGrDb[c] = proc.limGr[c].exchange (0.0f); limGrHold[c] = jmax (limGrHold[c], limGrDb[c]); }
    const int blocks = proc.blocksProcessed.load();
    const bool running = blocks != lastBlocks;
    lastBlocks = blocks;
    // audio present = this frame's input samples are not silent (not the meters: they fall slowly, which kept the
    // particles going for seconds after the sound stopped); the visualiser then holds and fades like the reference
    const bool audio = running && jmax (inMeter[0].rawPeakDb, inMeter[1].rawPeakDb) > -100.0f;
    const float mix = characterOn ? jlimit (0.0f, 1.0f, characterValue / 0.5f) : 0.0f;
    visualizer.advance (audio, mix, ap::Settings::get().animation(), ap::Settings::get().particleStyle() == ap::Settings::Particles::antiparticle);
    content.repaint();
    amountText.repaint();
}

void AntiparticleEditor::setTestLevels (float inDb, float outDb, bool audio)
{
    frozen = true;
    for (auto& m : inMeter) { m.barDb = inDb - 3.0f; m.peakDb = m.framePeakDb = inDb; m.peakHoldDb = inDb; m.rmsHoldDb = inDb - 3.0f; }
    for (auto& m : outMeter) { m.barDb = outDb - 3.0f; m.peakDb = outDb; m.peakHoldDb = outDb; m.rmsHoldDb = outDb - 3.0f; }
    if (audio) visualizer.advance (true, characterOn ? jlimit (0.0f, 1.0f, characterValue / 0.5f) : 0.0f, true,
                                   ap::Settings::get().particleStyle() == ap::Settings::Particles::antiparticle);
    content.repaint();
}

//==============================================================================
void AntiparticleEditor::Content::paint (Graphics& g)
{
    g.fillAll (Colours::black);
    ed.paintTopBar (g);
    ed.paintPanel (g);
    ed.paintPlate (g);
}

void AntiparticleEditor::Content::mouseDown (const MouseEvent& e)
{
    // clicking a readout resets the held readouts of that meter
    const auto p = e.position;
    if (p.y > 40.0f && p.y < 84.0f)
    {
        if (p.x < columnX[0]) for (auto& m : ed.inMeter) m.peakHoldDb = m.rmsHoldDb = -200.0f;
        if (p.x > columnX[4]) for (auto& m : ed.outMeter) m.peakHoldDb = m.rmsHoldDb = -200.0f;
        if (p.x > columnX[3] && p.x < columnX[4]) for (auto& v : ed.limGrHold) v = 0.0f;
        repaint();
    }
    // the band readouts (beside the GR meters)
    if (p.x > columnX[2] + 80.0f && p.x < columnX[3] && p.y > 110.0f && p.y < 262.0f)
    {
        for (auto& v : ed.bandGrHold) v = 0.0f;
        repaint();
    }
    if (ed.dropList != nullptr) ed.closePopups();
}

void AntiparticleEditor::paintTopBar (Graphics& g)
{
    g.setColour (theme::topBar);
    g.fillRect (0.0f, 0.0f, designW, 30.5f);
    g.setColour (theme::text);
    g.setFont (theme::font (12.0f));
    theme::textAt (g, "Preset", 136.0f, 19.5f);
    theme::textAt (g, "Sidechain", 500.5f, 19.5f);
    theme::textAt (g, "UI Scale", 747.5f, 19.5f);
}

void AntiparticleEditor::paintPanel (Graphics& g)
{
    const Rectangle<float> panel (1.5f, panelTop, designW - 3.0f, panelBottom - panelTop);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (panel, 15.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (panel, 15.0f, 2.2f);
    {
        // thin glass edge inside the outline, visible on the left
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (Rectangle<int> (0, 40, 9, 230));
        g.setColour (Colour (0xff8a8a8a));
        g.drawRoundedRectangle (panel.reduced (4.0f), 11.0f, 1.0f);
    }
    // faint backgrounds one grey level up (measured with the reference's dark levels lifted): the I/O columns, the
    // limiter's two GR tracks and the three band GR tracks
    {
        Graphics::ScopedSaveState s (g);
        Path inside;
        inside.addRoundedRectangle (panel.reduced (1.1f), 13.9f);
        g.reduceClipRegion (inside);
        g.setColour (Colour (0xff020202));
        g.fillRect (0.0f, 33.0f, columnX[0], 230.0f);
        g.fillRect (columnX[4] + 1.0f, 33.0f, designW - columnX[4], 230.0f);
        g.fillRect (732.0f, 90.0f, 12.0f, 170.0f);
        g.fillRect (768.0f, 90.0f, 12.0f, 170.0f);
        for (float y : { 84.5f, 149.5f, 214.5f }) g.fillRect (572.0f, y, 130.0f, 11.5f);
    }
    g.setColour (theme::divider);
    for (float x : columnX) g.fillRect (x, 36.0f, 1.0f, 224.5f);

    paintIoMeter (g, 0.0f, inMeter, true);
    paintIoMeter (g, 796.0f, outMeter, false);

    // EQ labels
    g.setFont (theme::font (11.25f));
    g.setColour (eqOn ? theme::mint : theme::plateText);
    const float baselines[3] { 125.5f, 190.5f, 255.5f };
    const char* names[3] { "high", "mid", "low" };
    for (int i = 0; i < 3; ++i)
        g.drawText (names[i], Rectangle<float> (103.0f, baselines[i] - 12.0f, 61.5f, 16.0f), Justification::centredTop, false);

    paintGainReduction (g);
    paintLimiter (g);
}

void AntiparticleEditor::paintIoMeter (Graphics& g, float x0, const Meter (&m)[2], bool target)
{
    const float centres[2] { x0 + 28.75f, x0 + 65.25f };
    g.setFont (theme::font (12.0f));
    for (int c = 0; c < 2; ++c)
    {
        g.setColour (m[c].peakHoldDb > 0.0f ? theme::clip : theme::mint);
        g.drawText (dbText (m[c].peakHoldDb), Rectangle<float> (centres[c] - 20.0f, 44.5f, 40.0f, 16.0f), Justification::centredTop, false);
        g.setColour (theme::mint);
        g.drawText (dbText (m[c].rmsHoldDb), Rectangle<float> (centres[c] - 20.0f, 67.5f, 40.0f, 16.0f), Justification::centredTop, false);
    }

    const float (&lx)[2] = ledX[x0 > 0.0f ? 1 : 0];
    // the input target lights while the input's peak sits in it: LEDs 32 .. 36, i.e. -10.5 .. -3 dBFS (measured with
    // up / down ramps of sine and noise: it follows the momentary peak, not the RMS bar; all three parts light together)
    const float framePeak = jmax (m[0].framePeakDb, m[1].framePeakDb);
    const bool lit = framePeak >= theme::ledThresholdDb (32) && framePeak < theme::ledThresholdDb (37);
    const bool inTarget[2] { lit, lit };
    for (int c = 0; c < 2; ++c)
    {
        int top = -1, peak = -1;
        for (int k = 0; k < theme::numLeds; ++k)
        {
            if (m[c].barDb > theme::ledThresholdDb (k)) top = k;            // strict: a -6.0 dB peak does not light the -6 LED
            if (m[c].peakDb > theme::ledThresholdDb (k)) peak = k;
        }
        g.setColour (theme::ledGreen);
        for (int k = 0; k <= top; ++k) g.fillRect (lx[c], 248.0f - 4.0f * (float) k, 8.0f, 1.0f);   // LEDs 1 px tall (measured)
        if (peak >= 0)
        {
            g.setColour (theme::mint);
            g.fillRect (lx[c], 248.0f - 4.0f * (float) peak, 8.0f, 1.0f);
        }
    }

    if (target)
    {
        // green target: two brackets and a line pair; filled while the level sits inside it
        // measured at 200 %: 1 px strokes, outer edges y 103.25 .. 122.75, corner radius 2.5; lit, each bracket is a solid
        // block rounded on its outer corners (y 103.25 .. 123.25) and the line pair becomes a solid block
        auto bracket = [&g] (float x, bool right, bool filled)
        {
            if (filled)
            {
                Path f;
                f.addRoundedRectangle (x, 103.25f, 8.0f, 20.0f, 2.5f, 2.5f, ! right, right, ! right, right);
                g.setColour (theme::targetFill);
                g.fillPath (f);
                return;
            }
            const float top = 103.75f, bottom = 122.75f, r = 2.5f;           // stroke centre lines (measured)
            const float ox = right ? x : x + 8.0f, ix = right ? x + 7.5f : x + 0.5f, d = right ? -r : r;
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
        bracket (15.0f, false, inTarget[0]);
        bracket (71.0f, true, inTarget[1]);
        if (lit)
        {
            g.setColour (theme::targetFill);
            g.fillRect (35.0f, 103.25f, 24.0f, 20.0f);
        }
        else
        {
            g.setColour (theme::targetLine);
            g.fillRect (35.0f, 103.25f, 24.0f, 1.0f);
            g.fillRect (35.0f, 122.25f, 24.0f, 1.0f);
        }
    }
}

void AntiparticleEditor::paintAmountText (Graphics& g)
{
    g.setColour (characterOn ? theme::mint : theme::plateText);
    g.setFont (theme::font (16.75f));
    theme::textAt (g, "amount", 364.0f, 120.5f);
    g.setFont (theme::font (41.5f));
    theme::textAt (g, String (roundToInt (characterValue * 200.0f)) + "%", 363.0f, 159.5f);
}

void AntiparticleEditor::paintGainReduction (Graphics& g)
{
    const float meterY[3] { 86.0f, 151.0f, 216.0f };
    const float targetX[3] { 681.0f, 621.0f, 653.0f };
    const float baselines[3] { 125.5f, 190.5f, 255.5f };
    const char* names[3] { "high", "mid", "low" };
    const auto labelCol = characterOn ? theme::mint : theme::dimLabel;
    g.setFont (theme::font (11.25f));
    for (int b = 0; b < 3; ++b)
    {
        g.setColour (labelCol);
        theme::textAt (g, names[b], 571.0f, baselines[b]);
        theme::textAt (g, grText (bandGrHold[b]), 701.5f, baselines[b], Justification::right);

        // 33 bars over 6 dB (MEASUREMENTS.md 6: 4.7 dB = 25 bars, 2.8 dB = 15); one bar stays lit while any is left
        const float gr = bandGrDb[b];
        const int bars = characterOn && gr > 0.005f ? jlimit (1, 33, (int) std::floor (gr * 33.0f / 6.0f)) : 0;
        const float tipX = 700.0f - 4.0f * (float) (bars - 1);
        // lit while the deepest bar ends inside the bracket (measured on the reference; it lags by a bar or so, hence
        // a bar of hysteresis once lit)
        const float slack = bandTargetLit[b] ? 4.0f : 0.0f;
        const bool hit = bars > 0 && tipX >= targetX[b] - slack && tipX <= targetX[b] + 19.0f + slack;
        bandTargetLit[b] = hit;
        // target: an upside-down U above the meter and a U below it; one filled block while the meter ends inside
        if (hit)
        {
            g.setColour (theme::targetFill);
            g.fillRoundedRectangle (targetX[b], meterY[b] - 10.0f, 19.0f, 28.5f, 2.5f);
        }
        else
        {
            g.setColour (characterOn ? theme::target : theme::dimLabel);
            Path p;
            // measured at 200 %: stroke centres x +0.25 / +18.25, y meterY - 9.25 / + 17.75, corner radius 3.5
            const float x = targetX[b] + 0.25f, w = 18.0f, yTop = meterY[b] - 9.25f, yBot = meterY[b] + 17.75f, r = 3.5f;
            p.startNewSubPath (x, meterY[b] - 2.0f);
            p.lineTo (x, yTop + r); p.quadraticTo (x, yTop, x + r, yTop);
            p.lineTo (x + w - r, yTop); p.quadraticTo (x + w, yTop, x + w, yTop + r);
            p.lineTo (x + w, meterY[b] - 2.0f);
            p.startNewSubPath (x, meterY[b] + 10.25f);
            p.lineTo (x, yBot - r); p.quadraticTo (x, yBot, x + r, yBot);
            p.lineTo (x + w - r, yBot); p.quadraticTo (x + w, yBot, x + w, yBot - r);
            p.lineTo (x + w, meterY[b] + 10.25f);
            g.strokePath (p, PathStrokeType (1.0f));
        }
        g.setColour (theme::mint);
        for (int k = 0; k < bars; ++k) g.fillRect (700.0f - 4.0f * (float) k, meterY[b], 1.0f, 8.5f);
    }
}

void AntiparticleEditor::paintLimiter (Graphics& g)
{
    const float centres[2] { 737.5f, 774.0f };
    g.setFont (theme::font (12.0f));
    for (int c = 0; c < 2; ++c)
    {
        g.setColour (limiterOn ? theme::mint : theme::dimLabel);
        g.drawText (grText (limGrHold[c]), Rectangle<float> (centres[c] - 20.0f, 67.5f, 40.0f, 16.0f), Justification::centredTop, false);
        if (limiterOn)
        {
            // the dashes grow downward with the reduction (one at rest; spacing and step estimated from the film)
            g.setColour (theme::mint);
            const int dashes = 1 + jlimit (0, 30, (int) std::floor (limGrDb[c] / 1.5f));
            for (int k = 0; k < dashes; ++k) g.fillRect (centres[c] - 4.0f, 90.0f + 4.0f * (float) k, 8.0f, 1.5f);
        }
    }
}

void AntiparticleEditor::paintPlate (Graphics& g)
{
    // the plate: a separate rounded panel (measured: x 0.5 .. 887.5, y 266.5 .. 358.75, corner radius 11) on a dark
    // ground, leather grain at the physical pixel (std ~3.9 grey levels, embossed: lag-1 correlation ~ -0.3)
    g.setColour (Colours::black);
    g.fillRect (0.0f, 265.0f, designW, designH - 265.0f);
    const Rectangle<float> plate (0.5f, 266.5f, designW - 1.0f, 92.25f);
    Path platePath;
    platePath.addRoundedRectangle (plate, 11.0f);
    {
        const float dpx = theme::pixelScale (g, content);
        if (std::abs (dpx - plateScale) > 0.001f || plateImage.isNull())
        {
            const int w = (int) std::ceil (plate.getWidth() * dpx), h = (int) std::ceil (plate.getHeight() * dpx);
            plateImage = Image (Image::RGB, w, h, false, SoftwareImageType());
            Image::BitmapData bd (plateImage, Image::BitmapData::writeOnly);
            Random rng (7);
            std::vector<float> noise ((size_t) (w * h));
            for (auto& n : noise) n = rng.nextFloat() + rng.nextFloat() + rng.nextFloat() - 1.5f;   // ~ gaussian
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
        // the panel above throws a short shadow on the plate's top edge
        g.setGradientFill (ColourGradient (Colour (0xff1e1d1b), 0.0f, 266.5f, Colour (0x00464540), 0.0f, 276.0f, false));
        g.fillRect (plate.withHeight (10.0f));
    }

    // shadows of the plate knobs: the light comes from the upper right, so they fall to the left and down
    // (a long soft shadow plus a tight contact shadow, fitted to the reference's plate brightness around the knobs)
    {
        // the measured shadow field (KnobShadow.h), rendered once per physical scale and drawn under each knob
        const float dpx = theme::pixelScale (g, content);
        constexpr float R = 25.06f, reach = 89.0f, half = R + reach;
        if (std::abs (dpx - knobShadowScale) > 0.001f || knobShadow.isNull())
        {
            const int px = (int) std::ceil (2.0f * half * dpx);
            knobShadow = Image (Image::ARGB, px, px, true, SoftwareImageType());
            Image::BitmapData bd (knobShadow, Image::BitmapData::writeOnly);
            for (int y = 0; y < px; ++y)
                for (int x = 0; x < px; ++x)
                {
                    const float dx = ((float) x + 0.5f) / dpx - half, dy = ((float) y + 0.5f) / dpx - half;
                    const float d = std::sqrt (dx * dx + dy * dy) - R;
                    if (d < -1.0f || d >= reach) continue;
                    const float a = std::atan2 (dx, -dy);                               // from up, clockwise
                    const float fade = jlimit (0.0f, 1.0f, (reach - d) / 15.0f);       // soft end, no ring
                    const float f = 1.0f - (1.0f - ap::knobshadow::factor (jmax (0.0f, d), a)) * fade;
                    bd.setPixelColour (x, y, Colours::black.withAlpha (jlimit (0.0f, 1.0f, 1.0f - f)));
                }
            knobShadowScale = dpx;
        }
        for (float cx : { 45.755f, 754.745f, 841.74f })
            g.drawImage (knobShadow, Rectangle<float> (2.0f * half, 2.0f * half).withCentre ({ cx, 313.11f }));
    }

    // labels
    g.setColour (theme::plateText);
    g.setFont (theme::font (12.0f));
    const std::pair<const char*, float> labels[] { { "input", 46.0f }, { "eq", 132.25f }, { "gain reduction", 636.75f }, { "limiter", 755.0f }, { "output", 841.75f } };
    for (auto& [t, x] : labels) theme::textAt (g, t, x, 280.0f, Justification::horizontallyCentred);

    // grille of embossed bars
    for (int r = 0; r < (int) std::size (grille); ++r)
    {
        const auto& row = grille[r];
        const bool even = (r % 2) == 0;
        const int count = even ? 16 : 15;
        const float first = even ? 187.25f : 199.25f;
        const float h = row.height * 0.55f + 1.1f;
        for (int k = 0; k < count; ++k)
        {
            const float cx = first + 24.0f * (float) k;
            const Rectangle<float> bar (cx - row.length * 0.5f, row.y - 0.2f, row.length, h);
            g.setColour (Colour (0xff1c1b18).withAlpha (0.55f));                      // cast shadow
            g.fillRoundedRectangle (bar.translated (0.0f, 0.9f).expanded (0.4f), h * 0.5f + 0.4f);
            g.setColour (Colour (0xff1f1e1b).withAlpha (0.9f));                       // dark rim
            g.fillRoundedRectangle (bar.expanded (0.45f), h * 0.5f + 0.45f);
            ColourGradient cg (Colour (0xff8a8a87), 0.0f, bar.getY(), Colour (0xff4e4d49), 0.0f, bar.getBottom(), false);
            cg.addColour (0.3, Colour (0xffe8e8e6));
            cg.addColour (0.6, Colour (0xff8c8c89));
            g.setGradientFill (cg);
            g.fillRoundedRectangle (bar, h * 0.5f);
        }
    }

    // lettering, spaced like the reference's sixteen glyph slots
    const String word = "THE ANTIPARTICLE";
    g.setFont (theme::font (9.5f));
    for (int i = 0; i < word.length(); ++i)
    {
        const String ch = String::charToString (word[i]);
        const float cx = 186.5f + 24.2f * (float) i;
        g.setColour (Colour (0xff161513));
        theme::textAt (g, ch, cx, 358.3f, Justification::horizontallyCentred);
        g.setColour (Colour (0xffd8d8d6));
        theme::textAt (g, ch, cx, 357.5f, Justification::horizontallyCentred);
    }
}

//==============================================================================
void AntiparticleEditor::closePopups()
{
    if (dropList != nullptr) content.removeChildComponent (dropList.get());
    dropList.reset();
    presetBox.open = scaleBox.open = false;
    presetBox.repaint(); scaleBox.repaint();
}

void AntiparticleEditor::showPresetList()
{
    closePopups();
    const auto all = proc.presets.list();
    std::vector<ap::DropList::Item> items { { "factory", true } };
    std::vector<int> map { -1 };
    for (int i = 0; i < (int) all.size(); ++i)
        if (all[(size_t) i].factory) { items.push_back ({ all[(size_t) i].name, false, all[(size_t) i].name == proc.presets.currentName() }); map.push_back (i); }
    items.push_back ({ "user", true }); map.push_back (-1);
    bool anyUser = false;
    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(size_t) i].factory) { items.push_back ({ all[(size_t) i].name }); map.push_back (i); anyUser = true; }
    if (! anyUser) { items.push_back ({ "-" }); map.push_back (-1); }
    dropList = std::make_unique<ap::DropList>();
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

void AntiparticleEditor::showScaleList()
{
    closePopups();
    std::vector<ap::DropList::Item> items;
    for (float s : scales) items.push_back ({ String (roundToInt (s * 100.0f)) + "%", false, std::abs (s - scale) < 0.01f });
    dropList = std::make_unique<ap::DropList>();
    dropList->setItems (items, 24.0f, false);
    dropList->onPick = [safe = Component::SafePointer<AntiparticleEditor> (this)] (int row)
    {
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
// settings page: oversampling ratio, I/O metering, particle animation (layout measured on the reference)
class SettingsDialog : public ap::Dialog
{
public:
    // the reference's three rows plus ours: particle style (god particle / antiparticle)
    SettingsDialog() : Dialog ("settings", { 234.0f, 50.5f, 420.0f, 300.0f }, false) {}
    void paint (Graphics& g) override
    {
        Dialog::paint (g);
        auto& st = ap::Settings::get();
        g.setColour (Colour (0xff050505));
        g.fillRect (box.getX(), 145.0f, box.getWidth(), 1.0f);
        g.fillRect (box.getX(), 236.5f, box.getWidth(), 1.0f);
        g.fillRect (box.getX(), 292.0f, box.getWidth(), 1.0f);
        g.setColour (theme::text);
        g.setFont (theme::font (16.75f));
        theme::textAt (g, "oversampling ratio", 258.0f, 118.5f);
        theme::textAt (g, "I/O metering", 258.0f, 182.0f);
        theme::textAt (g, "particle animation", 258.0f, 274.0f);
        theme::textAt (g, "particle style", 258.0f, 325.5f);
        const int ratios[4] { 1, 2, 4, 8 };
        int sel = 0;
        for (int i = 0; i < 4; ++i) if (ratios[i] == st.oversampling()) sel = i;
        segments (g, { 454.0f, 101.5f, 167.5f, 23.0f }, { "1", "2", "4", "8" }, sel);
        segments (g, { 454.0f, 257.5f, 84.0f, 23.0f }, { "ON", "OFF" }, st.animation() ? 0 : 1);
        segments (g, { 454.0f, 309.0f, 190.0f, 23.0f }, { "GOD PARTICLE", "ANTIPARTICLE" }, st.particleStyle() == ap::Settings::Particles::antiparticle ? 1 : 0);
        radio (g, 178.0f, "RMS & Peak", st.meterMode() == ap::Settings::Meter::rmsPeak);
        radio (g, 204.0f, "Short-term LUFS & Peak", st.meterMode() == ap::Settings::Meter::lufsPeak);
    }
    void mouseDown (const MouseEvent& e) override
    {
        auto& st = ap::Settings::get();
        const auto p = e.position;
        if (Rectangle<float> (454.0f, 101.5f, 167.5f, 23.0f).contains (p)) { const int r[4] { 1, 2, 4, 8 }; st.setOversampling (r[jlimit (0, 3, (int) ((p.x - 454.0f) / (167.5f / 4.0f)))]); repaint(); return; }
        if (Rectangle<float> (454.0f, 257.5f, 84.0f, 23.0f).contains (p)) { st.setAnimation (p.x < 496.0f); repaint(); return; }
        if (Rectangle<float> (454.0f, 309.0f, 190.0f, 23.0f).contains (p))
        {
            st.setParticleStyle (p.x < 549.0f ? ap::Settings::Particles::godParticle : ap::Settings::Particles::antiparticle);
            repaint(); return;
        }
        if (Rectangle<float> (450.0f, 168.0f, 190.0f, 20.0f).contains (p)) { st.setMeterMode (ap::Settings::Meter::rmsPeak); repaint(); return; }
        if (Rectangle<float> (450.0f, 194.0f, 190.0f, 20.0f).contains (p)) { st.setMeterMode (ap::Settings::Meter::lufsPeak); repaint(); return; }
        Dialog::mouseDown (e);
    }
private:
    static void segments (Graphics& g, Rectangle<float> r, StringArray labels, int sel)
    {
        const float w = r.getWidth() / (float) labels.size();
        g.setColour (theme::boxFill);
        g.fillRoundedRectangle (r, 4.0f);
        for (int i = 0; i < labels.size(); ++i)
        {
            const auto cell = Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
            if (i == sel)
            {
                Graphics::ScopedSaveState s (g);
                Path clip; clip.addRoundedRectangle (r, 4.0f);
                g.reduceClipRegion (clip);
                g.setColour (theme::text);
                g.fillRect (cell);
            }
            if (i > 0) { g.setColour (theme::text); g.fillRect (cell.getX() - 0.5f, r.getY(), 1.0f, r.getHeight()); }
            g.setColour (i == sel ? theme::boxFill : theme::text);
            g.setFont (theme::font (12.0f));
            g.drawText (labels[i], cell, Justification::centred, false);
        }
        g.setColour (theme::text);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }
    static void radio (Graphics& g, float cy, const String& text, bool on)
    {
        g.setColour (theme::text);
        g.drawEllipse (Rectangle<float> (13.0f, 13.0f).withCentre ({ 460.5f, cy }), 1.0f);
        if (on) g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre ({ 460.5f, cy }));
        g.setFont (theme::font (12.0f));
        theme::textAt (g, text, 478.0f, cy + 4.3f);
    }
};

// save preset: a name field and save / cancel
class SaveDialog : public ap::Dialog
{
public:
    SaveDialog (ap::PresetManager& pm, std::function<void()> saved)
        : Dialog ("save preset", { 240.0f, 74.0f, 408.0f, 209.0f }, true), presets (pm), onSaved (std::move (saved))
    {
        addAndMakeVisible (name);
        name.setBounds (Rectangle<float> (266.0f, 168.0f, 356.0f, 22.0f).toNearestInt());
        name.setFont (theme::font (16.0f));
        name.setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
        name.setColour (TextEditor::outlineColourId, Colours::transparentBlack);
        name.setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
        name.setColour (TextEditor::textColourId, theme::text);
        name.setColour (TextEditor::highlightColourId, Colour (0xff6b8890));
        name.setColour (CaretComponent::caretColourId, theme::text);
        name.setText (pm.currentName(), false);
        name.onReturnKey = [this] { commit(); };
        name.onEscapeKey = [this] { if (onClose) onClose(); };
    }
    void focusName() { name.grabKeyboardFocus(); name.selectAll(); }
    void paint (Graphics& g) override
    {
        Dialog::paint (g);
        g.setColour (theme::text.withAlpha (0.9f));
        g.drawRect (Rectangle<float> (258.0f, 146.5f, 371.5f, 47.0f), 1.0f);
        g.setColour (Colour (0xff8c8c8c));
        g.setFont (theme::font (12.5f));
        theme::textAt (g, "preset name", 267.5f, 164.5f);
        button (g, saveArea, "save");
        button (g, cancelArea, "cancel");
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (saveArea.contains (e.position)) { commit(); return; }
        if (cancelArea.contains (e.position)) { if (onClose) onClose(); return; }
        Dialog::mouseDown (e);
    }
private:
    void commit()
    {
        if (presets.save (name.getText())) { if (onSaved) onSaved(); }
        if (onClose) onClose();
    }
    static void button (Graphics& g, Rectangle<float> r, const String& t)
    {
        g.setColour (Colours::black);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::text);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setFont (theme::font (14.5f));
        g.drawText (t, r, Justification::centred, false);
    }
    const Rectangle<float> saveArea { 297.0f, 230.5f, 179.0f, 35.5f }, cancelArea { 501.0f, 230.5f, 89.5f, 35.5f };
    ap::PresetManager& presets;
    std::function<void()> onSaved;
    TextEditor name;
};
}

void AntiparticleEditor::showSettings()
{
    closePopups();
    auto d = std::make_unique<SettingsDialog>();
    d->setBounds (0, 0, (int) std::ceil (designW), (int) std::ceil (designH));
    d->onClose = [safe = Component::SafePointer<AntiparticleEditor> (this)]
    {
        MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->content.removeChildComponent (safe->dialog.get()); safe->dialog.reset(); } });
    };
    content.addAndMakeVisible (*d);
    dialog = std::move (d);
}

void AntiparticleEditor::showSavePreset()
{
    closePopups();
    auto d = std::make_unique<SaveDialog> (proc.presets, [this] { presetBox.text = proc.presets.currentName(); presetBox.repaint(); });
    d->setBounds (0, 0, (int) std::ceil (designW), (int) std::ceil (designH));
    d->onClose = [safe = Component::SafePointer<AntiparticleEditor> (this)]
    {
        MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->content.removeChildComponent (safe->dialog.get()); safe->dialog.reset(); } });
    };
    content.addAndMakeVisible (*d);
    auto* raw = d.get();
    dialog = std::move (d);
    raw->focusName();
}

void AntiparticleEditor::showDialog (std::unique_ptr<ap::Dialog> d)
{
    closePopups();
    if (dialog != nullptr) { content.removeChildComponent (dialog.get()); dialog.reset(); }
    d->setBounds (0, 0, (int) std::ceil (designW), (int) std::ceil (designH));
    d->onClose = [safe = Component::SafePointer<AntiparticleEditor> (this)]
    {
        MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->content.removeChildComponent (safe->dialog.get()); safe->dialog.reset(); } });
    };
    content.addAndMakeVisible (*d);
    dialog = std::move (d);
}

// the logo's menu, like Chan Sampler's (version, About, repository, issues, licence), drawn like the preset list
void AntiparticleEditor::showAboutMenu()
{
    if (dropList != nullptr) { closePopups(); return; }
    const String version = JucePlugin_VersionString;
    dropList = std::make_unique<ap::DropList>();
    dropList->setItems ({ { "The Antiparticle v" + version, true }, { "About The Antiparticle" }, { "The Antiparticle on GitHub" },
                          { "Report an Issue" }, { "License (AGPL-3.0)" } }, 24.0f, true);
    dropList->onPick = [safe = Component::SafePointer<AntiparticleEditor> (this), version] (int row)
    {
        if (safe == nullptr) return;
        safe->closePopups();
        if (row == 1)
        {
            const auto wrapper = safe->proc.wrapperType;
            const String format = AudioProcessor::getWrapperTypeDescription (wrapper);
            safe->showDialog (std::make_unique<ap::InfoDialog> ("about The Antiparticle", std::vector<std::pair<String, String>> {
                { "Version", version + (wrapper == AudioProcessor::wrapperType_Undefined ? String() : " (" + format + ")") },
                { {}, {} },
                { "What it is", "An open-source mix-bus processor modelled on and measured against Cradle's The God Particle" },
                { "License", "Free software under the GNU Affero General Public License v3" },
                { "Built with", "JUCE, the VST3 SDK and CLAP; Instrument Sans (SIL OFL). Their licenses come with it" },
                { "Source", repository.fromFirstOccurrenceOf ("https://", false, false) },
                { {}, {} },
                { "Note", "An independent project: not made, endorsed or supported by Cradle or Jaycen Joshua. The God Particle is a trademark of its owner." } }));
        }
        else if (row == 2) URL (repository).launchInDefaultBrowser();
        else if (row == 3) URL (repository + "/issues/new/choose").launchInDefaultBrowser();
        else if (row == 4) URL (repository + "/blob/main/LICENSE").launchInDefaultBrowser();
    };
    dropList->setBounds (4, 27, 214, 24 * 5 - 2);
    content.addAndMakeVisible (*dropList);
}
