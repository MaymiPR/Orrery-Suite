# The Antiparticle — Research

Phase 1 evidence for the first plugin of the collection: **The Antiparticle**, an open-source recreation of Cradle's
**The God Particle** (mix-bus / master channel processor made with mix engineer Jaycen Joshua). Everything here was
written before any DSP or UI code.

"Local" sources are the user's own installed copy of The God Particle, read for analysis. Nothing from it (images,
scripts, fonts, presets, renders, screenshots) is copied into this repository: those files live in the git-ignored
`reference/` folder.

## Sources

| Id | Source | What it gives |
|---|---|---|
| **W1** | Official product page, fetched 2026-10-22: [cradle.app/products/the-god-particle](https://cradle.app/products/the-god-particle) | marketing description, formats, system requirements, price |
| **W2** | Official support article [GUI Overview](https://support.cradle.app/hc/en-us/articles/4411750433044-GUI-Overview) (updated 2024-04-10), with an annotated 200 % screenshot | the closest thing to a manual: every control described |
| **W3** | [How do I use The God Particle?](https://support.cradle.app/hc/en-us/articles/6423895706004-How-do-I-use-The-God-Particle) | intended use, meaning of the green targets |
| **W4** | [What Is The God Particle?](https://support.cradle.app/hc/en-us/articles/4411773820692-What-Is-The-God-Particle) | positioning |
| **W5** | [Updates and Release notes](https://support.cradle.app/hc/en-us/articles/6423691528468-Updates-and-Release-notes) (updated 2026-10-20) | version history 1.0.2 → 1.2.5, settings page added in 1.2.2 |
| **W6** | [Oversampling](https://support.cradle.app/hc/en-us/articles/15947880896532-Oversampling) | 1x / 2x / 4x / 8x, latency grows with oversampling |
| **W7** | [LUFS Metering](https://support.cradle.app/hc/en-us/articles/15947889022484-LUFS-Metering) | RMS & Peak or Short-term LUFS & Peak readouts |
| **W8** | [How do I export stems using The God Particle?](https://support.cradle.app/hc/en-us/articles/12637616897940-How-do-I-export-stems-using-The-God-Particle) (updated 2025-01-31) | what the side-chain drives (and what it doesn't) |
| **W9** | FAQ: [targets](https://support.cradle.app/hc/en-us/articles/6424147405076-Why-doesn-t-my-track-hit-all-of-the-gain-reduction-targets), [inner workings](https://support.cradle.app/hc/en-us/articles/6424232645908-Can-you-tell-me-more-about-what-s-going-on-inside-The-God-Particle), [presets](https://support.cradle.app/hc/en-us/articles/6424187550868-Are-there-any-presets-included-in-The-God-Particle) | defaults = Jaycen's settings, Cradle won't publish internals, preset save/load |
| **W10** | [KVR product page](https://www.kvraudio.com/product/the-god-particle-by-cradle/details) (secondary) | version 1.2.4 listed, formats, price; nothing beyond W1 |
| **L1** | Local `C:\Program Files\Common Files\VST3\Cradle The God Particle.vst3` — `Contents/Resources/moduleinfo.json` | class IDs, version 1.2.4, VST 3.7.8 SDK, category Fx |
| **L2** | `tools/ParamDump.cpp` (this repo) run on L1 → `reference/params_48k.txt`, `reference/params_41pt.txt` | every host parameter: names, ranges, defaults, display text; buses, latency, editor size |
| **L3** | Plug-in state from L2 (`reference/state_default_component.bin`, decoded from JUCE's base64) | names of the internal blocks and how they are grouped |
| **L4** | Printable strings / RTTI class names of the L1 binary (`reference/strings.txt`) | engine (Cradle engine 0.4.5, Lua 5.4 UI), embedded resource names, fonts, DSP node library |
| **L5** | Screenshots of the real editor at 100 % (Wavy's `MeterCapture`, 50 Hz tone bursts) → `reference/editor_f*.png` | current (1.2.4) layout, 888 × 359 px |
| **L6** | `%LOCALAPPDATA%\Cradle\logs\app.cradle.TheGodParticle\log.log` | engine version, build id 8d2af976 (2024-07-04), Lua sprite warnings |

Not done on purpose: the UI/DSP Lua scripts and the global settings file are stored in an encrypted container
(L4: `PublicKeyPimpl`, hashed class names, `global.json` / `app.cradle.TheGodParticle.json` are encrypted blobs).
We do not decrypt them. Behaviour is found by measuring the plug-in from the outside (Phase 3).

## Product facts (W1, W4, W10)
- Cradle, $119. Formats: macOS AU / VST3 / AAX, Windows VST3 / AAX (no CLAP, no Linux). Licensing through Cradle Hub,
  internet needed (W1). Installed version here: 1.2.4 (Hub says 1.2.5; W5 notes 1.2.5 only changed macOS
  certificates and still reports 1.2.4).
- Described as Jaycen Joshua's whole master-channel chain in one plug-in: "a multitude of processing algorithms",
  harmonic processing, an **adaptive limiter** that keeps dynamics and transients at commercial loudness, and input /
  gain-reduction targets for gain staging and a balanced EQ (W1).
- Designed for the master bus, also for drum / instrument busses and single tracks (W3, W4).
- Defaults = Jaycen's own settings, unchanged on every mix since 2021; he does all adjustments **before** the
  plug-in's input (W3, W9).

## Documented controls (W2, with L2 for the numbers)

Signal order as described in W2 and confirmed by the state's grouping (L3):
**Input gain → EQ (low / mid / high) → Character ("Amount": multiband dynamics + processing chain) → Limiter → Output gain**.

| Control (UI) | Host parameter (L2) | Range / display (L2) | Default | Documented behaviour (W2 unless noted) |
|---|---|---|---|---|
| **input** knob (bottom left) | `Input Gain` | −12 … +12 dB, linear in the normalised value (2.4 dB per 0.1) | 0 dB | level into the plug-in. Meter: peak readout (red when > 0 dBFS) + RMS readout, both reset by clicking; green target = where Jaycen's mix input lands |
| **eq high / mid / low** knobs | `EQ High Gain`, `EQ Mid Gain`, `EQ Low Gain` | −6 … +6 dB, linear | 0 dB | "air" / body / low-end warmth; meant to steer each band to its gain-reduction target before the character section |
| eq power button | `EQ Bypass` | 2 steps (0 = active) | active | bypasses the EQ |
| **amount** knob (large, centre) | `Character` | 0 … 1 shown in the host; UI shows **0 … 200 %** (0.5 = 100 %) | 100 % | how much "character" is applied: the whole processing chain behind the UI, whose key part is a **multiband dynamics processor** (3 bands: high / mid / low) |
| amount power button | `Character Bypass` | 2 steps | active | bypasses the character section; W5 1.2.2: lower CPU while bypassed |
| gain reduction meters + readouts (high / mid / low) | — | dB | — | attenuation per band; green target per band (Jaycen's mix-bus settings) |
| **Sidechain INT / EXT** (top bar) | none (host-only, saved as `hostOnly.sidechain`, L3) | INT / EXT | INT | EXT: the side-chain bus drives the multiband dynamics. W8: the side-chain only feeds the **character** dynamics; the limiter always detects its own input |
| **limiter** knob (bottom) | `Limiter Input Gain` | 0 … +12 dB, **non-linear** taper (table below) | +5 dB (0.494206) | gain into an "intelligent adaptive limiter": louder with more gain, never above 0 dBFS; two readouts = limiter GR (L / R) |
| limiter power button | `Limiter Bypass` | 2 steps | active | bypasses the limiter |
| **output** knob (bottom right) | `Output Gain` | −12 … +12 dB, linear | 0 dB | final level after the limiter (keep at 0 dB to avoid clipping). Meter like input |
| Preset menu, ← →, disk icon | — | factory "default", user presets | default | save with the disk icon; user presets under "user" (W9) |
| UI Scale menu | — (stored in `prefs.uiScaling`, L3) | 100 % / … / 200 % | 100 % | window zoom |
| Settings (gear, added 1.2.2) | — (global, encrypted file) | Oversampling 1x / 2x / 4x / 8x; meters RMS or Short-term LUFS; animation on / off | to be read in Phase 2/3 | W5, W6, W7. Oversampling adds latency (W6) |
| — | `Bypass` | Off / On | Off | host bypass (VST3 bypass parameter) |

### Limiter Input Gain taper (L2, 41 points)
Normalised → dB: 0 → 0.000, 0.25 → 2.351, 0.494206 → 5.000 (default), 0.5 → 5.068, 0.75 → 8.242, 1 → 12.000.
Not a power law (JUCE skew fits only to ±0.21 dB); `13.96·(e^(0.6193·p) − 1)` fits to ±0.025 dB, a cubic
`8.830p + 2.038p² + 1.126p³` to ±0.006 dB (display precision is 0.001). Phase 3 will dump 1001 points and pick the
exact law; until then the table itself is the reference.

### Host-level facts (L1, L2)
- One effect class, `The God Particle`, vendor Cradle, plug-in code `Tgpj`, manufacturer code `Crdl` (L3).
- Buses: stereo in, stereo out, plus a **side-chain** input bus (off by default). Mono → mono is **not** supported.
- **Latency** reported at default settings: 179 samples @ 44.1 kHz, 189 @ 48 kHz, 357 @ 96 kHz (not a fixed time:
  ~4.0 / 3.9 / 3.7 ms; Phase 3 measures it per sample rate and oversampling factor). Tail: 0.
- No VST programs; state ≈ 2.5 kB (XML with Cradle's own snapshot text).
- Editor: **888 × 359 px** at 100 %, not resizable by dragging (scale menu only).

## Internal structure (L3, L4) — hints, to be confirmed by measurement

The saved state names the blocks (L3):
```
hostOnly: inputGain, outputGain, sidechain          (not part of presets)
preset.signalChain:
    oversampled: lowEQMacro, midEQMacro, highEQMacro, eqBypassMacro, characterMacro (0.5), characterBypass
    masterLimiter: preGain (5), bypass
```
So the **EQ and the character section run inside the oversampled part**, and the **master limiter sits after it**
(possibly at the base rate). Input / output gain are outside presets: loading a preset keeps them.

The engine (Cradle engine 0.4.5) is a general node library shared by all Cradle products, so the list of linked
classes (L4) shows what *could* be used, not what The God Particle uses. Relevant ones: `Crossover2…5`,
`FeedforwardCompressor` / `FeedbackCompressor` (property names `threshold`, `attackTime`, `releaseTime`, `kneeWidth`,
`levelDetectorMode`, `makeUpGain`, `sidechainEnabled`), `UpwardCompressor`, `Expander`, `BrickwallLimiter` /
`Maximizer` (`ceiling`, `lookaheadTime`, `releaseTimeHigh`, `releaseTimeLow`, `releaseSwitchThreshold` — a two-speed
release matches "adaptive limiter"), `ShelvingFilter` / `PeakingEqualizer` (SVF), `DynamicShelvingFilter`,
`DynamicPeakingEqualizer`, `ParametricEnhancer`, `TransientShaper`, `SpectralTransientShaper`, `StereoWidener`,
`AsymmetricTanhWaveshaper`, `PolynomialWaveshaper`, `Clipper`, `Distortion`, `SubharmonicSynthesizer`,
`LinearLoudnessGain`, `LoudnessMeter` (`momentaryLoudness`, `shortTermLoudness`), `DCBlocker`, and the oversampler
`OversampledAudioGraph<PolyphaseFIRUpsampler, PolyphaseFIRDownsampler>` (`numOversamplingStages` → 2x per stage).
Also linked: `NeuralModeler` (LSTM / WaveNet, "experimental"). Whether any of these are in TGP's graph is a Phase 3
question.

Third-party code visible in L4 (for our own notices, nothing is reused): Lua 5.4 + sol2, nlohmann/json, spdlog,
Crypto++, libcurl, Expat, AVIR (image resizing), libpng.

## GUI (W2 screenshot, L4 resource names, L5 screenshots)

Layout at 100 % (888 × 359), current version 1.2.4 (L5), described here for orientation; Phase 2 measures every
pixel:
- **Top bar** (black): Cradle logo icon (left), `Preset` label + drop-down (`default`), ← / → arrows, disk (save)
  icon, `Sidechain` label + `INT | EXT` segmented toggle (selected segment white), `UI Scale` drop-down (`100%`),
  settings gear (right).
- **Main panel** (black, light-grey rounded outline): six columns separated by thin vertical lines:
  1. input meter: peak readouts L/R (top), RMS readouts L/R, bracket-shaped green target, two green LED bar meters;
  2. EQ: mint-green power button, three small knobs labelled `high`, `mid`, `low`;
  3. centre "particle" visualiser (rounded stadium shape, animated 30 fps sprite: coloured particles; a white
     variant and an "off" image exist) with the large `amount` knob, `amount` caption and big `100%` readout
     (mint), and a mint power button;
  4. gain reduction: per band label (`high` / `mid` / `low`), GR readout, green target brackets, small horizontal
     LED meters (`Components/meters/gainReductionHorizontal.lua`);
  5. limiter: mint power button, GR readouts L/R and meters;
  6. output meter like the input one.
- **Bottom plate** (dark warm grey, baked shadows image): knobs `input`, `limiter`, `output`, labels `eq` and
  `gain reduction` over their columns, a grille of horizontal slots with "THE GOD PARTICLE" spaced lettering in the
  middle.
- Embedded images (L4, names only): `Cradle_TheGodParticle_UI_{Large,Medium,Small}Knob_V1.png` + matching `…Pip`
  pointer images, `BackplateBakedShadows_V2.jpg`, `VisualizerColor_V1_30FPS_JPGSPRITE.jpg`,
  `VisualizerWhite_V1_30FPS_JPGSPRITE.jpg`, `VisualizerOff_V1.jpg`, `Cradle_LogoIcon_512x512.png`.
- Fonts (L4): **Whyte** Regular / Light / Light Italic (commercial typeface by Dinamo — cannot be shipped) and
  **Source Code Pro** Regular (SIL OFL — can be shipped). Phase 2 needs a free substitute for Whyte with matching
  metrics.
- The UI is already dark; the only alternate appearance in the resources is the visualiser (colour / white / off,
  W5: animation on/off toggle). "Dark-mode counterpart" therefore means: keep this dark scheme, and use the
  colour/white visualiser states as the reference does.
- The older W2 screenshot (1.2.1 era, 200 %) shows the same layout without the gear and with a static visualiser;
  the current layout (L5) is the target.

## Naming and branding

| Original | Recreation | Product folder / target |
|---|---|---|
| Cradle **The God Particle** | **The Antiparticle** (renamed from Devil's Particle on 2026-10-22) | `TheAntiparticle` |

The look (layout, sizes, colours, knob style) is recreated with our own drawing code. Cradle's and Jaycen Joshua's
names, the Cradle logo, the "THE GOD PARTICLE" lettering and all shipped images and fonts are replaced by our own
("THE ANTIPARTICLE" lettering: 16 characters, one per letter slot of the reference; the maker's mark replaced by the Orrery Suite mark, a sun on its orbit with the orbit's lower arc lit as a crescent and a planet, drawn flat in the same place, size and grey). The repo states clearly that it is an independent project, not affiliated
with Cradle.

## Open questions for Phase 3 (measurement)
Answered by the Phase 3 measurements in [MEASUREMENTS.md](MEASUREMENTS.md); what is still open is listed in its section 8.

1. EQ: band types (shelf / bell / tilt), corner frequencies and Q; does each knob scale a fixed curve (macro)?
2. Character: crossover frequencies and slopes; per-band threshold / ratio / knee / attack / release; peak or RMS
   detector; how `Amount` maps onto them (0 … 200 %: threshold, ratio, mix?); any saturation / harmonic stage
   (THD vs level and frequency), stereo processing (M/S, width), transient shaping, make-up gain.
3. Limiter: ceiling (output peaked at −0.70 dBFS in the L5 capture: ≈ −0.7 dBFS ceiling?), look-ahead,
   two-speed release law ("adaptive"), true-peak or sample-peak, linked or per-channel (two GR readouts).
4. Latency per sample rate and oversampling factor; does the oversampling setting change the sound (filters, aliasing)?
5. Parameter smoothing times for each knob and the bypass switches.
6. Meters: peak / RMS / short-term LUFS ballistics, GR meter ballistics, target positions.
7. Where the 1.2.2 settings (oversampling, meter mode) are stored, so the harness can render at a known setting.
8. Side-chain EXT: confirm only the character dynamics follow it.

## Phase 2: GUI measurements (L5 + `tools/RefCapture`, 200 % screenshots)

All numbers in design pixels (= pixels at 100 %); the reference renders vector-sharp at 200 %, so everything was
measured there and halved. Screenshots of the reference stay in `reference/` (git-ignored); our editor is compared
with `AntiparticleSnapshot` + `tools/ui_compare.py` (mean absolute RGB difference over the window: 8.4 / 255 at
200 %, most of it in the animated / photographic parts and in anti-aliased text edges).

| Element | Measured |
|---|---|
| Window | 888 × 359.5 (heights truncate: 359 at 100 %, 719 at 200 %); UI Scale 50 / 75 / 85 / 100 / 125 / 150 / 200 %, saved per instance. Ours can also be resized freely by dragging its bottom-right corner (three light grooves cut into the plate, brighter under the mouse) or the host's window edge: proportions fixed, 50 … 200 %, the UI Scale box shows the result. While the size changes, the per-scale images (glass, plate, shadows, knob bodies) are drawn stretched and rendered sharp again once it settles (150 ms or on release): about 9 ms a frame instead of 107 (AntiparticleSnapshot --bench) |
| Top bar | 0 … 30.5, #050607. Text #E7F1F4, 12 px. Preset box (176, 3, 193 × 24.5) fill #121211, 1 px #7F7E7A; ← → and save boxes 24 × 24.5 with a #E7F1F4 frame; INT/EXT (560, 3, 85 × 24.5), selected half filled #E7F1F4; UI Scale box (794, 3, 66 × 24.5); gear #7F7E7A |
| Main panel | rounded outline r ≈ 15, 2 px #C0C0C0, black inside; column dividers at x 89 / 176 / 559 / 711 / 798, 1 px #3C3C3C |
| Mint | #8EDBC4: readouts, labels, power buttons (72 × 24.5, filled = on, 1 px outline = bypassed) |
| I/O meters | readouts at x 28.75 / 65.25 (input) and 824.75 / 861.25 (output), peak baseline 56.5, RMS 79.5. 40 LEDs per channel, 8 × 1 on a 4 px pitch, bottom LED at y 248; **LED k lights above −58.5 + 1.5 k dB** (strictly above: a −6.0 dB sine peak lights LED 34, not 35). Green #00824B bar = plain RMS (150 ms; a sine reads 3 dB under its peak); above it a mint run up to the fast peak (1 ms / 50 ms follower); one mint LED at the peak readout's held value, not drawn above 0 dBFS (MEASUREMENTS.md 6). Input target: brackets at x 15 / 71, 8 wide, 1 px strokes centred on y 103.75 / 122.75, corner radius 2.5, and a 1 px line pair (x 35 … 59) on the same lines, #00824B; lit, the brackets become solid blocks (y 103.25 … 123.25) rounded on their outer corners and the line pair a solid block. It fills with the fast peak (the top of the mint run, either channel): fully from −12 to −3 dBFS, fading over 1.5 dB (one LED) at each end; all three parts fill together (measured frame by frame against the reference's own LEDs) |
| EQ | knobs R 20.375 at (133.375, 90.75 / 155.75 / 220.75) (outer edge, found by maximising inside/outside contrast along the circle); labels 11.25 px at baselines 125.5 / 190.5 / 255.5; labels turn #BCBABB while the EQ is bypassed |
| Amount | knob R 50.875 at (292.875, 148.25); "amount" 16.75 px at (364, 120.5); value 40 px-class digits at baseline 159.5, 0 … 200 %; text #BCBABB while bypassed |
| Gain reduction | labels at x 571, values right-aligned at 701.5 (same baselines as the EQ); meters = 1 × 8.5 bars on a 4 px pitch growing left from x 700 at y 86 / 151 / 216; targets 19 wide at x 681 (high), 621 (mid), 653 (low), 1 px strokes centred on meterY − 9.25 / + 17.75, corner radius 3.5, bars = floor(5.5 × GR) plus one marker bar at the readout's held deepest GR on a 20 dB scale (1.65 bars / dB); each target fills with the band's GR, fully over 0.34 … 1.38 / 2.45 … 3.98 / 0.97 … 2.46 dB (high / mid / low), fading over ~0.22 dB at each end; lit, the upper and lower U fill separately and the bars pass between (MEASUREMENTS.md 6); labels, values and targets #808080 while the character section is bypassed |
| Limiter | readouts at 737.5 / 774 (baseline 80), mint dashes at y 90; #808080 while bypassed |
| Visualiser | glass racetrack centred (367.75, 148.25), straight half-length 75.25, out to r 113; its lighting measured as a field over the track (`tools/fit/fit_glass.py`: 25 stations per straight, 144 per end (1.25°), 0.5 px radial steps; the lit glass as the median of the active frames, ≈ grey 37 near the pill, 15 at the outer edge). Ours is a vector drawing shaped to that look (`tools/fit/draw_glass.py`, `glass_params.json` → `resources/glass/*.png`): two rim lines and faint sheen lines with profiles along the track, the reflections as soft-edged polygons with smooth quartic shading, and for the lit glass the sparks' light as coloured vertical streaks from our own seeded noise with the measured radial falloff, held back by the reflections. Unlit it matches the measured field to a mean of about 2 grey levels; the lit streak pattern is our own. Catmull-Rom interpolation in the plug-in keeps the reflections' slanted edges smooth; greyscale version when the sparks are white. Amount pill: centre (368.1, 148.9), half-length 74.7, r 49.75 (not concentric with the glass), black inside, ~0.7 px edge of grey ≈ 130. Sparks (filmed with RefCapture, tracked frame to frame): a free-running **1 s loop** at 30 frames/s that does not restart with the audio (its motion does not follow the level; how many are lit does, below); ~290 sparks per frame at 100 %; counter-clockwise drift faster further out (−70 / −89 / −108 px/s at r 53–70 / 70–90 / 90–112) with large scatter, outer sparks pulled inwards; lifetimes mostly under 0.2 s (up to ~0.5 s); hue shares white 34 %, yellow 20 %, cyan-blue 16 %, blue 10 %, orange 9 %, violet 7 %, magenta/red 4 %; size (blob area at 100 %) quartiles 1 / 3 / 9 px², 90th 27. The light follows the **input level** (after the input gain; the output gain has no effect), filmed with sine and pink-noise staircases, a 32 s fade, stops from −6, −30, −45, −55 and −65 dBFS and the end of a mastered track. It is a peak meter that jumps up at once and falls ~90 dB/s, with no hold. Full brightness down to −40 dBFS, dark at −75. With v = (meter + 75) / 35, the glass glow is v^1.6 of its full brightness (sine steps: 78 % at −45, 59 % at −50, 43 % at −55, 28 % at −60, 15 % at −65). The sparks go out sooner: about half are lit at −55, nearly none at −60. So a quiet ending dims with the music, and the light after a stop holds as long as the meter takes to fall to −40: 0.40 s from −6 dBFS, 0.12 s from −30, none from −45. The two layers then fade over ~0.4 s (sparks ~0.25 s; the glow 69 % after 0.08 s, 36 % after 0.18 s, 17 % after 0.28 s). Ours: the same meter, glow v^1.6, sparks ((v − 0.4) / 0.6)^0.7, falling with real elapsed time. Glow within ~1 grey level of the reference at every step and the track's ending, stops within a frame. The meter reads this frame's raw input samples, not the slowly falling meters; colour mix = amount / 100 % (white at 0 % or bypassed); "particle animation" off = static frame |
| Plate | rounded panel x 0.5 … 887.5, y 266.5 … 358.75, corner radius 11, on black; #46453F with an embossed leather grain at the device pixel (std ≈ 3.9 grey levels, lag-1 correlation ≈ −0.3); labels #BCBABB 12 px at baseline 280; knobs R 25.06 at y 313.11 (x 45.755 / 754.745 / 841.74); their shadow is a smooth fitted model (`tools/fit/fit_knob_shadow.py` → `KnobShadow.h`: 3-harmonic angle amplitude × two exponentials, so it cannot ripple): light from the upper right, darkest toward 240° (factor ~0.3 at the edge, 0.57 at 16 px, 0.72 at 24 px, 0.86 at 40 px); grille 19 staggered rows on a 24 px pitch, bar length 21 → 7 |
| Knobs | brushed metal, light fixed, pointer turns −135 … +135°. Lighting fitted per knob size on the median of three knob positions (16-harmonic angle series × radial profile; rim as 23 bands 0.005 R apart, 8 harmonics each) → `Source/ui/KnobModel.h` (numbers only) |
| Mouse | JUCE's own slider behaviour (RefCapture drag scripts, `nulltest/uiknobs.py`). Vertical drag, **absolute from the press point**: 1 % of the range per design pixel (full range in 100 px at every UI scale), so after overshooting an end the mouse has to come back that far before the value moves (up 80 px then back 40 from 0 dB: +9.6 dB). **Shift: velocity mode**, cursor hidden: each mouse move of d px changes the value by 0.2 · (1 + sin(π (1.5 + min(0.5, max(0, d − 1) / 200)))) of the range (JUCE's defaults: sensitivity 1, threshold 1, offset 0; d = 2 … 200 px per move matches to 0.001 dB), so slow moves fine-tune and fast ones jump, at most 20 % per move. **Ctrl: the normal speed** (pressed before or during the drag). Double-click or Alt-click: default value. Ours adds Ctrl (Cmd) as a second key for the velocity mode. |
| Value readout | while the mouse is over a knob's cell, or the knob is being dragged (wherever the mouse goes), its caption shows the value instead of its name: `%.1f dB` (`3.6 dB`, `-3.6 dB`, `-0.0 dB` for −0.006, no plus sign), same font, colour, size and baseline as the caption, centred the same way. It switches within one repaint (< 12 ms) both ways, no fade. Cells: the knob's column, 87 px wide centred on the knob; on the plate its full height (from y 266.5), for the EQ knobs from 20.5 px above the knob centre to 45 px below. The amount knob keeps its own readout. |
| I/O link (ours) | an `I/O LINK` toggle in the top bar (664, 3, 64 × 25), centred between the side-chain switch and the `UI Scale` label, drawn like a segment of INT / EXT (filled while linked); saved with the session. Linked, the output gain is always the input gain's opposite: linking snaps the output to −input, a gesture on either knob moves the other (both captions show their values during it, host gestures for both), host automation and presets are mirrored by the editor, and the processor itself applies output = −input while linked (so automating only the input keeps the output mirrored even with the editor closed). |
| Settings dialog | (234, 50.5, 420 × 255.5); oversampling 1 / 2 / 4 / 8, I/O metering RMS & Peak / Short-term LUFS & Peak, particle animation ON / OFF; defaults 1, RMS & Peak, ON; global (all instances) |
| Save dialog | (240, 74, 408 × 209): "preset name" field, save / cancel |
| Preset list | FACTORY / USER captions (spaced capitals, #C8785A), factory "default", user presets by name |
| RMS readout (Phase 3) | live, not held (a step from −6 to −20 dB reads −20.9 after 2.5 s), and not plain RMS: a −6 dBFS sine reads −7.1 (20–100 Hz: −6.9 … −7.1), −7.2 (1 kHz), −8.1 (2, 5, 20 kHz), −7.4 (8, 16 kHz, where the sampled peak is −7.3), −8.7 (12 kHz); noise at RMS −12 dB reads −10.4. It tracks the sampled peaks; formula still open |
| Faint backgrounds | with the dark levels lifted ×12: the I/O columns, the limiter's two GR tracks (x 732–744, 768–780) and the three band GR tracks (x 572–702) are one grey level above the panel |
| Logo menu (ours) | like Chan Sampler's (version header, About panel, repository, Report an Issue, License (AGPL-3.0)), drawn as the editor's own drop list (same look as the preset list). The reference's logo opens Cradle's about screen |
| Particle style (ours) | settings page row "particle style": GOD PARTICLE (the reference's sparks: hue shares and colours fitted per group, crisp points with a moderate glow, the fastest blurred into short dashes over 1/120 s) or ANTIPARTICLE (bubble chamber: gamma rays turn into particle / antiparticle pairs whose tracks curl apart and spiral in, cyan and magenta; pairs annihilate in a flash with two back-to-back gamma rays; a counter-flow of matter / antimatter specks; violet glass; 3 s loop; white at amount 0 %) |

Font: Whyte is replaced by **Instrument Sans** Regular (SIL OFL, `resources/fonts`), picked by comparing rendered
words ("amount", "Sidechain", "high", "Preset") with the reference: closest widths and shapes of five candidates.
