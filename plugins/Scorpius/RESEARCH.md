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
- Buses: stereo in, stereo out, **no side-chain**. Mono → mono is not supported.
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
- Faders: caps 37 × 22.5, brushed metal with a radial sheen and a groove, travelling from y 237.25 (level 0) to 85.25
  (level 1); the track is mint below the cap while the path is on.
- The plate grille: 19 staggered rows (21 / 20 dots), a halftone of an arch: the dot area follows
  0.043 + 0.932 exp(−((d − 239.3) / 26.3)²) of the largest, d the distance from (300, 587.8), fitted to every dot.
- The stripe under the columns is black with a soft grey floor (y 291 … 297), a faint line at its foot and a cyan glint
  at its left end; particles stream through it while audio plays (to be matched in the UI rounds).
- Settings: one row, particle animation ON / OFF, in a 420 × 254 box.

Ours, not the reference's: the drive colour and the compression mode are host parameters (`Drive Color`,
`Comp Mode`), the window can be resized from its corner, and the maker's mark is the Orrery Suite mark.
