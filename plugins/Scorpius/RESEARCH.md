# Scorpius — Research

Phase 1 evidence for the second plug-in of Orrery Suite: **Scorpius**, an open-source recreation of Cradle's
**Orion**, a drum-bus processor made with mix engineer Jaycen Joshua. Everything here was written before any DSP or UI code.

"Local" sources are the user's own installed copy of Orion, read for analysis. Nothing from it (images, model files,
scripts, fonts, presets, renders, screenshots) is copied into this repository: those files live in the git-ignored
`reference/orion/` folder.

## Sources

| Id | Source | What it gives |
|---|---|---|
| **W1** | Official product page, fetched 2026-10-25: [cradle.app/products/orion](https://cradle.app/products/orion) | positioning, formats, system requirements, price |
| **W2** | Official support article [GUI Overview](https://support.cradle.app/hc/en-us/articles/30831917426708-GUI-Overview) (created 2024-09-30) | every section described |
| **W3** | [How do I use Orion?](https://support.cradle.app/hc/en-us/articles/30831954160404-How-do-I-use-Orion) | intended use, meaning of the input target |
| **W4** | [What is Orion?](https://support.cradle.app/hc/en-us/articles/30831901369876-What-is-Orion) | positioning, relation to The God Particle |
| **W5** | [KVR product page](https://www.kvraudio.com/product/orion-by-cradle) (secondary) | listing only |
| **L1** | Local `C:\Program Files\Common Files\VST3\Cradle Orion.vst3` — `Contents/Resources/moduleinfo.json` | class IDs, version 1.2.1, VST 3.7.8 SDK, category Fx |
| **L2** | `tools/ParamDump` run on L1 → `reference/orion/params_{48k,41pt,44k,96k}.txt` | every host parameter: names, ranges, defaults, display text; buses, latency, editor size |
| **L3** | Plug-in state from L2 (`reference/orion/state_default_component.bin`, decoded with `tgpstate.py`) | the preset structure, including settings the host cannot see |
| **L4** | Printable strings / RTTI class names of the L1 binary (`reference/orion/strings.txt`) | engine version, embedded resource names (images, presets, a neural model file), fonts, DSP node library |
| **L5** | Screenshots of the real editor at 100 % (`tools/RefCapture`, 50 Hz tone bursts) → `reference/orion/editor_*.png`, menus and settings page → `ui_*.png`, every factory preset → `preset_*.png` + `presets_dump.txt` | current (1.2.1) layout, 888 × 403 px; every preset's settings |
| **L6** | `%LOCALAPPDATA%\Cradle\logs\app.cradle.Orion\log.log` | engine 0.5.10, build 3be92cfe (2025-01-17), UI tree names |
| **L7** | `C:\ProgramData\Cradle\apps\app.cradle.Orion.json`, install log | Hub lists 1.2.2 as current; the installed build is 1.2.1 (VST3 + AAX installed) |

Not done on purpose: as with The God Particle, the UI / DSP Lua scripts, the global settings and the embedded neural
model are in Cradle's encrypted / private containers. We do not decrypt or extract them. Behaviour is found by
measuring the plug-in from the outside (Phase 3).

## Product facts (W1, W2, W4)
- Cradle, $119 (bundle with The God Particle: "Beyond Super Analog"). Formats: macOS AU / VST3 / AAX, Windows VST3 /
  AAX (no CLAP, no Linux). Licensing through Cradle Hub. Installed version here: 1.2.1 (Hub offers 1.2.2).
- "A drum bus processor created in collaboration with" Jaycen Joshua (W4); defaults are how he starts his drum mixes
  (W1). "Engineered to complement" The God Particle (W1), with the same principles and the same kind of interface (W4).
- Use (W3): set the input so the loudest section of the drums lights the green input target; the input level drives
  how much compression, saturation and character every module produces.
- Reviews note heavy CPU use (W5 search results); worth measuring, since our version should be lighter.

## Documented controls (W2, with L2 for the numbers, L3 / L5 for the hidden ones)

Signal order, from the panel layout and W2 (to be confirmed in Phase 3):
**Input gain → Drive (+ hi-pass) → Harmonics → Compression → Lift → Harmonic EQ → Parallels (sub, low, loud, grit,
mixed in) → Output gain**.

| Control (UI) | Host parameter (L2) | Range / display (L2) | Default (preset "start me up") | Documented behaviour (W2 unless noted) |
|---|---|---|---|---|
| **input** knob (plate, left) | `Input Gain` | −12 … +12 dB, linear (2.4 dB per 0.1) | 0 dB | level into Orion. Meter: peak readout (red over 0 dBFS) and RMS readout, both reset by clicking; green target bracket = intended level |
| drive power | `Drive On/Off` | 2 steps | on | |
| drive **hi pass** | `Drive Hi-Pass` | 2 steps | on | a high-pass filter after the drive. The drive icon shows stars when on, "+" marks when off (L5) |
| drive **color a / b / c** | **none** (state `drive.driveMode`, L3) | a / b / c | a | three colours, "each offering its own distinct characteristic" |
| harmonics power | `Harm On/Off` | 2 steps | on | |
| harmonics **freq 50Hz / 80Hz** | `Harm Freq` | 2 steps: 50 / 80 | 80 Hz | "adds excitement and energy to the kick drum and other low-frequency information", focused at 50 or 80 Hz |
| **harmonics** knob | `Harm Amount` | 0 … 1 (UI shows %, e.g. 80.0 %) | 0.8 | amount. A deforming circle above the knob follows the processing (L5) |
| compression power | `Comp On/Off` | 2 steps | on | |
| compression **mode a / b** | **none** (state `ids.compression.mode`, "Mode A" / "Mode B", L3) | a / b | b | two modes |
| **compression** knob | `Comp Amount` | 0 … 1 | 0.9 | "a mix control which applies more or less compression"; Jaycen aims for −1 … −2 dB of gain reduction. A numeric GR readout above it (L5) |
| lift power | `Lift On/Off` | 2 steps | on | |
| **lift** knob | `Lift Amount` | 0 … 1 | 0.8 | "saturation and harmonic excitement across the full frequency spectrum", with "an additional focus on the higher frequencies" (presence, transient detail). A small deforming circle above it (L5) |
| harmonic EQ power | `EQ On/Off` | 2 steps | **off** | |
| **40Hz, 100Hz, 1kHz, 3kHz, 10kHz** knobs | `EQ 40Hz` … `EQ 10kHz` | 0 … +6 dB, linear (boost only) | 0 dB | "five fixed bands that allow you to boost and adjust harmonic content"; "small 1dB increments" matter. When on, a response curve is drawn above the knobs (L5) |
| **sub, low, loud, grit** faders, each with a power button | `Par Sub On/Off` / `Par Sub Level`, … `Par Grit …` | on/off; level 0 … 1 | all off, level 0 | parallel paths: **sub** more impact in the sub range; **low** weight in the low end (boom on kicks, warmth on bass); **loud** more perceived loudness and presence without raising the output; **grit** accentuates transients |
| **output** knob (plate, right) | `Output Gain` | −12 … +12 dB, linear | 0 dB | final level. Meter like the input's |
| Preset menu, ← →, disk icon | — | factory / user list (L5) | start me up | same menu as The God Particle |
| **Compare A → B** (top bar) | **none** (state `stateSwitch.selectorValue`, L3) | A / B; → copies A to B | A | two complete settings in one instance |
| UI Scale menu | — (`prefs.uiScaling`, L3) | 50 / 75 / 85 / 100 / 125 / 150 / 200 % (L5) | 100 % | window zoom |
| Settings (gear) | — (`prefs.animationEnabled`, L3) | particle animation ON / OFF (L5) | on | the only setting: no oversampling or LUFS options (unlike The God Particle) |
| — | `Bypass` | Off / On | Off | host bypass |

Note: the host-reported defaults of the on/off switches (L2: all 1, EQ 40 Hz … 0) are the parameters' own defaults;
the plug-in opens with the factory preset "start me up" (L3), whose values are the "Default" column.

### Factory presets (L4 names, L5 values)

Nine presets. Settings that differ from "start me up" (drive a, hi-pass on, harmonics 80 Hz 0.8, compression mode b
0.9, lift 0.8, EQ off, parallels off):

| Preset | Differences |
|---|---|
| c mode | drive color **c**, compression mode **a** |
| easy does it | drive color **c**, compression mode **a**, lift **0.5** |
| free at last | compression **off** |
| fruity | input **+9.12 dB**, compression mode **a** **0.76**, lift **0.7**, grit **on 0.189** |
| louder | harmonics **50 Hz**, lift **1.0**, loud **on 0.731** |
| low end theory | hi-pass **off**, harmonics **50 Hz**, EQ **on, 40 Hz +6 dB** |
| parallel me in | harmonics **1.0**, compression **1.0**, lift **1.0** |
| start me up | (the defaults) |
| top of the morning | drive, harmonics, compression and lift **off** (amounts 1.0), grit **on 0.731** |

### Host-level facts (L1, L2)
- One effect class, `Orion`, vendor Cradle, plug-in code `Orio`, manufacturer code `Crdl` (L3).
- Buses: stereo in, stereo out, **no side-chain**. Mono → mono is not supported. (Scorpius adds a side-chain: UI
  round 5.)
- **Latency: 0 samples at 48 kHz, 31 at 44.1 kHz, 48 at 96 kHz** (L2). The DSP appears to run at a fixed 48 kHz and
  resample other rates (L4: `ResampledAudioGraph<FixedSampleRatePolicy>`, and the neural model below is trained at
  48 kHz). Phase 3 confirms this, and checks other rates (88.2, 192 kHz).
- No VST programs; state ≈ 3.1 kB. Tail 0.
- Editor: **888 × 403 px** at 100 % (wider plate than The God Particle's 888 × 359), not resizable by dragging.

## Internal structure (L3, L4) — hints, to be confirmed by measurement

The saved state names the blocks (L3):
```
preset:
    input.gain, output.gain
    drive:       onOff, hpfOnOff, driveMode ("a" | "b" | "c")
    ids:         harmonics {onOff, freq (50 | 80), amount}
                 compression {onOff, mode ("Mode A" | "Mode B"), amount}
                 lift {onOff, amount}
    eq:          onOff, gain1 … gain5
    parallels:   sub / low / loud / grit {onOff, level}
stateSwitch.selectorValue (A / B), prefs {uiScaling, animationEnabled}
```
So harmonics, compression and lift form one group (`ids`), separate from the drive, the EQ and the parallels. Unlike
The God Particle, input / output gain are part of the preset ("fruity" sets the input to +9.12 dB).

The most important hint: the binary embeds a **neural model**,
`resources/modelFiles/Harmonics - FULL WET - 48kHz - LSTM_1_LAYER_NO_MASK_NY32K 400.nam` (L4): a Neural Amp Modeler
file, a one-layer LSTM (the engine's `FixedSizeLSTM<1, 24>`: hidden size 24) trained at 48 kHz on a fully wet
"Harmonics" process. So at least the **Harmonics** module is a learned black-box model of some (probably analog)
process, mixed with the dry signal by its amount. We cannot ship or reuse Cradle's model. A recreation has to model
the same behaviour from measurements, as its own LSTM trained on our captures, or as a fitted structure (band
filter → waveshaper → filter) if that is close enough.

The engine (0.5.10, newer than The God Particle's 0.4.5) links the same general node library; the linked classes show
what *could* be used: `Crossover2…5`, `JaycenJoshuaCrossover` (experimental), `FeedforwardCompressor` /
`FeedbackCompressor`, `UpwardCompressor`, `Expander`, `BrickwallLimiter`, `ShelvingFilter` / `PeakingEqualizer` /
`MultiModeFilter` (SVF), `ButterworthFilter`, `LadderFilter`, `CircuitBentFilter`, `LouBellL3Filter` (experimental),
`DynamicShelvingFilter`, `DynamicPeakingEqualizer`, `ParametricEnhancer`, `TransientShaper`,
`SpectralTransientShaper`, `StereoWidener`, `AsymmetricTanhWaveshaper`, `PolynomialWaveshaper`, `Clipper`,
`Distortion`, `SubharmonicSynthesizer` (`evenHarmonicsLevel`), `EqualPowerBlend`, `LinearLoudnessGain`,
`NeuralModeler` (LSTM, WaveNet) and the oversampler `PolyphaseFIRUpsampler / Downsampler`. Plausible matches to
check in Phase 3: drive colours = waveshapers (asymmetric tanh / polynomial / clipper); **sub** = subharmonic
synthesizer; **grit** = transient shaper or clipper; **loud** = upward compression; harmonic EQ = peaking filters
feeding a saturator ("boost harmonic content").

## GUI (W2, L5 screenshots, L4 resource names, L6)

Layout at 100 % (888 × 403), version 1.2.1 (L5), for orientation; Phase 2 measures every pixel:
- **Top bar** (black): Cradle logo icon, `Preset` label + drop-down, ← / → arrows, disk (save) icon, **`Compare`
  label + `A (→) B` segmented toggle** (where The God Particle has its side-chain toggle), `UI Scale` drop-down,
  settings gear.
- **Main panel** (black, light-grey rounded outline), columns separated by thin vertical lines:
  1. input meter: peak and RMS readouts L/R, green target bracket, two LED bar meters (as The God Particle's);
  2. **drive**: mint power button, an animated cluster of stars ("+" marks with hi-pass off; the log names
     `visualizer.star.N` / `visualizer.plus.N`), `hi pass` toggle, `color` a / b / c selector;
  3. **harmonics, compression, lift**: three power buttons; above the harmonics knob a circle that deforms with the
     signal, above compression a large numeric GR readout (`-3.6`), above lift a small deforming circle; `freq`
     50Hz / 80Hz and `mode` a / b selectors; three medium knobs;
  4. **harmonic eq**: outlined power button (off by default), five small knobs (100Hz, 3kHz on top; 40Hz, 1kHz,
     10kHz below); a response curve appears when it is on;
  5. **parallels**: four power buttons over four vertical faders with a metal cap (`sub`, `low`, `loud`, `grit`);
  6. output meter.
- A **visualiser stripe** across the full width under the columns (`centralVisualizerStripe`, 38 px tall, L6):
  coloured particles and white streaks while audio plays, dark when idle.
- **Bottom plate** (warm grey): `input` and `output` knobs at the ends, labels `drive`, `harmonic eq`, `parallels`, a
  grille of dots with **"O R I O N"** spaced lettering under it.
- Embedded images (L4, names only): `Cradle_Orion_UI_{Backplate,Visualizer}_V1.jpg`,
  `Cradle_Orion_UI_{MediumKnob,MediumKnobPip,SmallKnob,SmallKnobPip,Fader}_V1.png`, plus the shared UI library's
  sliders and the Cradle logo.
- Fonts (L4): **Whyte** Regular / Light / Light Italic (commercial, cannot be shipped) and **Source Code Pro**, the
  same as The God Particle. Our Instrument Sans substitute and most of The Antiparticle's widgets (preset menu, scale
  menu, power buttons, meters, readouts, dialogs, knobs) carry over.

## Naming and branding

| Original | Recreation | Product folder / target |
|---|---|---|
| Cradle **Orion** | **Scorpius** (chosen 2026-10-25) | `Scorpius` |

As The Antiparticle answers The God Particle with its opposite, Scorpius answers Orion: in the myth the scorpion is
the hunter's opposite, and the constellation Scorpius rises as Orion sets, on the far side of the sky. The look is
recreated with our own drawing code. Cradle's and Jaycen Joshua's names, the Cradle logo, the "ORION" lettering, the
images, the fonts and the neural model are replaced by our own ("S C O R P I U S" lettering under the grille, eight
letters where the original spaces five). The repo states that it is an independent project, not affiliated with Cradle.

## Open questions for Phase 3 (measurement)

1. Signal order and which modules are parallel; does the input level change the processing as documented (W3)?
2. The fixed 48 kHz rate: the resampler, its latency and response at 44.1 / 88.2 / 96 / 192 kHz.
3. Drive: the three colours' transfer curves (static and with memory), level dependence, the hi-pass (cutoff, slope),
   stereo behaviour.
4. Harmonics: what the LSTM does (harmonic spectrum against level and frequency, 50 vs 80 Hz focus, how much memory),
   the amount law (wet / dry?), and whether a fitted structure reaches a good null or our own LSTM is needed.
5. Compression: modes A / B (detector, threshold, ratio, knee, attack, release, stereo link), the amount as a mix
   (W2), the GR readout's quantity.
6. Lift: full-band saturation with high-frequency focus — filter + shaper structure, amount law.
7. Harmonic EQ: band shapes and Q at each frequency, whether it feeds a saturator, the curve drawn in the UI.
8. Parallels: sub (subharmonic synthesis?), low, loud (upward compression?), grit (transients?) — each path's
   processing and how its level mixes in.
9. Parameter smoothing, switching clicks, A / B switching (instant or crossfaded).
10. Meters and readouts (expected to match The God Particle's measured ones), the deforming circles and the
    visualiser's response to level.
11. CPU use, as a target to beat.

## Phase 2: GUI measurements (L5 + `tools/RefCapture`, 200 % screenshots)

The editor was measured on 200 % captures of the reference (its state's `prefs.uiScaling` set to 2 with
`nulltest/orionstate.py`, which edits any value of the state by its path) with `tools/measure_ui.py` (filled boxes,
outlines, silver regions and their centres) and `tools/measure_text.py` (every word's box), then compared region by
region with our own snapshots (`tools/compare_ui.py`, `ScorpiusSnapshot`). Findings:
- The top bar is The God Particle's, with `Compare` and its A / B switch where that one has `Sidechain`; the window is
  888 × 403.5 design px (403 at 100 %).
- Columns divide at x 86, 175, 424, 608, 801. Power buttons are 72 × 23.5 (the parallels' 38 wide) at y 41.5; the
  selectors are rows of 22.5 px stacked 0.5 apart, each outlined on its own; labels sit on baseline 263.5 (upper EQ row
  192.5, freq / mode 194), mint whether their section runs or not.
- The six column knobs are one size (r ≈ 20.1), the plate knobs are The God Particle's (r 25.06). Knob centres were
  taken from their silver regions, corrected as on The God Particle's knobs (known exactly), then checked against our
  snapshot to 0.1 px.
- I/O meters as The God Particle's, lower: LEDs on a 4 px pitch from y 258, readouts at x 30.5 / 60.5 (output 827.5 /
  857.5); the input target is narrower (lines x 36 … 54) and higher (y 100.5 … 120).
- The compression readout shows a reduction as "- 3.6" (a gap after the minus) and rest as "0.0". The sparks, the two
  circles and the readout are not drawn while their module is off.
- The harmonic EQ's curve (drawn while the EQ is on): a log axis from 40 Hz at the left edge to about 24 kHz over
  x 432 … 600, 6 dB = 50 px above a baseline at y 124; the bands are drawn about 1.4 octaves wide at half height.
- Faders: the cap is one image (the four are identical), 36.9 × 22.9, drawn 0.12 px left of the track, travelling
  from y 237.5 (level 0) to 85.5 (level 1); the track is mint below the cap while the path is on. See UI round 3.
- The plate grille: 19 staggered rows (21 / 20 dots), a halftone of an arch: the dot area follows
  0.043 + 0.932 exp(−((d − 239.3) / 26.3)²) of the largest, d the distance from (300, 587.8), fitted to every dot.
- The stripe under the columns: see UI round 3.
- Settings: one row, particle animation ON / OFF, in a 420 × 254 box.

Ours, not the reference's: the drive colour and the compression mode are host parameters (`Drive Color`,
`Comp Mode`), the window can be resized from its corner, and the maker's mark is the Orrery Suite mark.

## UI round 3: animations, the copy message, the fader cap, the stripe (RefCapture films at 200 %)

`tools/RefCapture` gained `drag=x,y,0,0,burst,<ms>` (click, then grab the editor as fast as it can, ~70 frames/s, each
frame named with its time since the click) and `rows=<from>,<to>` (film only a band of rows). The same scripts were
run on the reference and on ours, and compared.
- **Drive marks:** each of the five marks keeps its identity and size through a, b and c (a → b → c → a brings every
  mark home), and a new colour moves each one in a straight line to its new place, easing in and out (cubic) over
  215 ms; their paths cross. The hi-pass cross-fades the two shapes over ~110 ms (the old one fades out under the new
  one). The drive's power button fades the marks out or in over ~85 ms.
- **Module displays** (the circles, the GR readout, the EQ curve, the marks) fade linearly over ~85 ms with their power
  buttons, also when a preset or a compare slot switches them; knobs, faders and selectors jump.
- **Copy message:** Compare's arrow (only it) shows "A copied to B." (or "B copied to A.") in a box 200 × 60.5 at
  (344, 337) over the plate: fill #050607, a 1 px edge #252523, the text as the labels (12 px, baseline 34.25 below the
  box's top). It is there at once, holds 0.55 s, then fades out linearly over 1.05 s.
- **Mouse on knobs and faders:** Shift = JUCE's velocity mode (cursor hidden, fine moves), Ctrl = normal speed,
  Alt-click = default value, on knobs and faders alike.
- **Fader cap:** fitted per pixel (`tools/fit_fadercap.py` → `FaderCap.h`, 4.8 grey levels rms): a rounded rectangle
  with four one-pixel rim rings (each a table over the angle round the cap), a spun-metal face whose grey depends only
  on the angle round its centre (period 180°: darkest ~92 a little right of up, brightest ~207 a little below the
  right), a darker ring round the groove, and the groove (grey 45, its edge 58) 31.95 × 2.06 across the middle.
- **Stripe, idle:** a divider at y 272.5 … 273.5 (the column dividers run down to it); a capsule outline (grey ~12)
  x 11.75 … 874.5, y 283.75 … 298.25, radius ~4; a flat grey-27 floor y 290.75 … 297 along its lower half, a little
  green towards the right; a short cyan-grey glint (54, 66, 69) inside the left end at y 285.25; a soft highlight
  (~150) inside the right end; under it the panel's inner edge as a dim band (~36) at y 303 … 305.5.
- **Stripe, playing** (`tools/fit_stripe.py`, a probe of 1 kHz tones and level steps):
  - brightness follows the input's peak level: dark below −67.5 dBFS, full from −40 up, linear in dB between (0.27 at
    −60, 0.43 at −55, 0.59 at −50, 0.78 at −45); the level it follows rises at once and falls at 50 dB/s, so after a
    −10 dBFS tone stops the light stays full 0.6 s and fades over ~0.55 s, after −40 it fades at once. (One case does
    not fit: a slow ramp up from −60 dBFS stays dim all the way, then lights fully once it stops.) With the particle
    animation off, the stripe stays dark;
  - a fixed glow: the 20th percentile of the lit frames is separable, a profile down the stripe (brightest at y 289)
    times a bell along it (centre x ≈ 452, Gaussian width ≈ 227 plus a sharp core), fitted to 8.8 grey levels rms;
  - particles: ~1600 local maxima per frame, spread almost evenly along x 55 … 815 and across y 272.5 … 306 (dense in
    286 … 302); dashes in the band keep to three one-pixel lanes (y 289.1, 291.6, 294.25, a weaker one at 302.5);
    isolated dots are ~1 × 1 px cores with a faint halo; colours: about half nearly white, the rest amber, cyan,
    blue-violet, red-orange, azure, pink-red (in that order of frequency), a few flares (rounded, 2 … 3.5 wide, up to
    ~14 tall) swelling and shrinking within ~0.15 s;
  - motion: frame-to-frame correlation of the particle layer 0.36 at 26 ms, 0.23 at 53 ms; in the band particles slide
    sideways either way at ~±150 px/s, above it they rise and below it they sink (up to ~100 px/s).
  Ours (`Source/ui/Stripe.cpp`) draws the fitted glow and ~1600 particles sampled from these distributions, adds them
  as light per physical pixel and matches these statistics (mean light 47 vs 46 grey levels, particle-layer
  correlation 0.34 vs 0.35 at one frame, the brightness after every stop within ~0.1).

## UI round 4: the circles, the EQ display, the GR readout, the stripe's level (RefCapture films at 200 %)

- **Harmonic EQ display:** not a response curve. Each band is its own shape (filled with mint at 18 %, outlined at
  37 %; overlaps add up), heights linear in dB (51 … 53 px at 6 dB) over a baseline at y 124.75. The bands are evenly
  spaced across the section: 100 Hz, 1 kHz, 3 kHz are bells exp(−(|x − c| / 15.64)^2.47) at x 478.98, 515.48, 551.49;
  40 Hz and 10 kHz are shoulders 1 / (1 + e^(±(x − c) / 6.4)) past x 457.97 and 575.05, drawn over x 432.25 … 600.75
  (each band alone at 3 and 6 dB, fitted to its outline).
- **Harmonics and lift circles:** at rest circles of radius 6.43. They follow a band's level: harmonics the lows (flat
  to 80 Hz, −7 dB at 100 Hz, nothing by 150 Hz), lift the mids (−24 dB at 300 Hz, −15 at 500, most at 1 … 2 kHz,
  −15 at 3 kHz, −31 at 6 … 8 kHz), measured with a sweep of sines at −12 dBFS. Neither the amounts nor the 50 / 80 Hz
  switch nor anything but the input gain changes them. The level is a sum of spectrum magnitudes (a 4096-point FFT
  fits: pink noise at −15 dBFS RMS reads 0.52 in the mids and 0.27 in the lows, where a sine of the same RMS reads its
  amplitude), so noise and music grow them more than a tone. Radius 9.32 + 15.84 (a^0.4 − 0.083) above −52 dBFS
  (9.6 at −50, 10.7 at −40, 18 at −10), up within ~20 ms, back down at ~62 px/s. The outline is irregular (about 2 px
  rms at any size) and moves only as the level changes: a steady tone freezes it.
- **Compression readout:** moves towards the gain reduction while there is some (−1.5 at −10 dBFS, −1.3 at −12 for
  a 1 kHz tone, reached in ~0.4 s) and holds its last value while there is none, through silence too. Ours drives it
  with a stand-in fitted to these tones until the compressor exists (Phase 4).
- **The stripe's level, explained:** the level the light follows changes only when the input jumps (a step of 6 dB
  or more, up or down) and when the sound stops (then it takes the last level and fades from it at 50 dB/s). Smooth
  changes leave it where it was: a ramp up from −60 dBFS stays dim even after it levels off at −20 for 2 s; a ramp
  down from −10 stays full; a steady tone keeps its brightness for as long as it lasts (6 s tested). Ours does the
  same and follows the reference's brightness through all four probes (mean difference 0.03 … 0.04).
- **Stripe colours:** in the middle the particles are mostly white where they overlap; along the stripe amber is the
  commonest colour (13 … 22 %), then cyan (9 … 16 %) and red-orange; the ends carry ~60 particles per frame each.
- **CPU:** the editor redrew the plate's ~400 shaded grille dots on every repaint (meters 30 times a second); now they
  are drawn once per scale, and the stripe, circles and meters move on one 40 Hz clock (the reference draws ~37
  frames a second). Playing at 200 %: 0.96 → 0.12 of a core (the reference: 1.36).

Ours, not the reference's: a second particle style, **SCORPIUS** (settings page; Source/ui/Sky.cpp): the constellation
from its star positions (east to the left), twinkling, Antares swelling on each beat, the tail in a band of the Milky
Way, venom running like meteors from the head to the stinger.

## UI round 5 (ours)

- **Top bar shared with The Antiparticle:** Compare's A / B switch moved left of `Preset` (no label) and a `Sidechain`
  INT / EXT switch sits where The God Particle has it, with I/O LINK between it and `UI Scale`. The two top bars are
  now the same pixel for pixel (only the preset's name differs). The reference has Compare at x 604 … 734.
- **Side-chain (ours; the reference has none):** a stereo side-chain bus (off unless the host enables it). EXT feeds
  the compressor's detector from it instead of the signal it compresses; with the bus off, EXT hears silence. The
  switch is saved with the session and is not part of a preset or a compare setting (as The Antiparticle's). Until
  the compressor exists (Phase 4) it drives the gain-reduction stand-in (`dsp/Visuals.h`).
- **SCORPIUS style, played by the music:** the processor now also reads 24 bands (30 Hz … 16 kHz, log-spaced) from
  the circles' spectrum. Each band's level is taken against its own recent loudest (falling 4 dB/s, held within 24 dB
  of the loudest band), so quiet mixes play too; hits are detected in three groups (kick 30 … 144 Hz, snare
  315 Hz … 4.3 kHz, hi-hats 5.6 … 16 kHz: 3 dB over their 0.35 s average and rising). What they drive:
  - an aurora spreading both ways from Antares, lows next to it in crimson out to highs in gold at the ends (magenta,
    violet, blue, cyan, mint between): each part as tall and bright as its band, waving, in shimmering rays,
    flaring when its band jumps 5 dB over its average;
  - kick: Antares swells, a ring spreads from it, a pulse of light runs out along the body; the Milky Way breathes;
  - snare: venom from the head to the stinger, which flares and bursts into coloured sparks;
  - hi-hats: dust stars glint in colour; each main star brightens and flashes with the band at its place.
  The brightness still follows the reference's rules (the same as the ORION style). Filmed offline with
  `ScorpiusSnapshot --film=audio.f32,seconds,every` (the stripe every n frames of the 40 Hz clock). Cost at 200 %:
  3.5 ms a frame (the ORION style 2.8).
