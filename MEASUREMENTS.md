# Measurements (Phase 3)

What the reference, **Cradle The God Particle 1.2.4** (VST3, Windows), does to audio. Everything here was measured
by rendering test signals through it offline. The DSP was not decompiled and Cradle's encrypted scripts were not
opened. The numbers are the targets for The Antiparticle's DSP (Phase 4).

**Data and plots stay local.** The data and plots live in `measurements/` and the renders in `nulltest/renders/`.
Both folders are git-ignored by the project rule, so this file is the published record. Every number below can be
reproduced with the scripts named in each section.

## How it was measured

| Piece | What it does |
|---|---|
| `tools/RefRender.cpp` | Offline JUCE VST3 host. Renders float32 files through a plug-in at a fixed rate and block size. Can set parameters (by displayed value or normalised), automate a parameter at a block boundary, load a saved state and feed a stereo side-chain. Each job gets a fresh instance; `raw=1` skips latency compensation. |
| `tools/RefCapture.cpp` | Opens the editor and plays audio in real time. It films the editor (every ~27 ms) or clicks and drags in it. Used here for the meters and to change the global oversampling setting. |
| `nulltest/tgp.py` | Python harness: `render_many(jobs)`, stage presets (`EQ_ONLY`, `CHAR_ONLY`, `LIM_ONLY`, `NONE`), test signals, and an on-disk render cache. |
| `nulltest/measure_*.py`, `fit_*.py`, `plot_char.py` | One script per topic. Each writes JSON and PNGs to `measurements/<topic>/`. |

**Conventions:**
- 48 kHz, block 512 (block 64 for the smoothing tests).
- Default settings unless noted, with oversampling at 1x (the reference's default).
- Each stage is isolated by bypassing the other two with their own power buttons.
- Every render starts with 2 s of pre-roll, because the reference's internal delay ramps from 54 to 152 samples over
  about 0.25 s after loading.
- "Gain" means the level of the test tone's fundamental (lock-in, Hann window) at the output re the input.

## 1. Signal path and latency

| Fact | Value |
|---|---|
| Order | input gain → EQ → character → limiter → output gain (the EQ and character run inside the oversampled block) |
| Reported latency, 1x | 179 / 189 / 357 samples at 44.1 / 48 / 96 kHz |
| Reported latency at 48 kHz, 1x / 2x / 4x / 8x | 189 / 212 / 217 / 219 samples |
| Core path, every stage bypassed | a fixed **3.1505 ms** delay: an integer delay plus a first-order Thiran all-pass for the fraction (151 samples + a = 0.5636 at 48 kHz, i.e. 151.279 samples; the same time at 44.1, 88.2, 96 and 192 kHz, `measure_core.py`). A step rings at Nyquist and overshoots by 24 % (1.226) before settling. Magnitude is flat. With the character section on, its FIR crossover carries this delay instead (3.1) |
| Limiter on | adds 38 samples (its look-ahead): 190 samples raw |
| Character / EQ on | no extra delay |
| Quirk | With the limiter bypassed, the output is 37–38 samples earlier than the reported latency (the report does not change with the limiter bypass). |
| Defaults, small signal | +8.44 dB at 1 kHz (character +3.54 dB, limiter +4.90 dB) |

## 2. EQ (`measure_eq.py`, `fit_eq.py`)

- Three RBJ-cookbook biquads at the processing rate:
  - low shelf 180 Hz, Q 0.7
  - peaking 637 Hz, Q 0.5
  - high shelf 10 000 Hz, Q 0.7
- The gain in dB equals the knob value (±6 dB, linear).
- The fit at 1x is exact: max magnitude error ≤ 0.0004 dB, and the phase matches too.
- The response does not depend on level (impulses at −40 and −12 dBFS agree).
- With oversampling the filters are designed at the higher rate, so the top octave differs. High shelf +6 dB at
  15 kHz: 5.56 dB (1x), 5.09 (2x), 4.98 (4x), 4.95 (8x).

## 3. Character section (`measure_char_*.py`, `fit_crossover.py`, `plot_char.py`)

### 3.1 Bands

**Method.** A −3 dBFS masker tone was played over a comb of 44 probe tones at −60 dBFS. The probes the masker
pushes down show which band it falls in (`bands_masker.png`).

**Result.** Three bands from a **Linkwitz-Riley 4th-order** crossover at **157 Hz and 2.68 kHz**:
- low = LP(fc1) · AP(fc2)
- mid = HP(fc1) · LP(fc2)
- high = HP(fc1) · HP(fc2)

Fitted to all seven maskers at once, the median error is 0.02 dB. LR2 fits ten times worse.

**Phase 4 correction: the crossover is FIR, not IIR** (`fit_bands.py`).
- The section's impulse response is finite: 661 taps at 48 kHz, with the main peak at 151. That peak is the core delay,
  so the 3.15 ms delay is this crossover's latency, kept even when the section is bypassed.
- The small-signal impulse responses at the nine amount settings span **exactly three** fixed filters (the fourth
  singular value is 2e-8 of the first).
- The split into three bands is not unique from these data alone. It was chosen closest to linear-phase
  Linkwitz-Riley 4th-order magnitudes at 157 / 2676 Hz, which matches the masker test.
- These three IRs rebuild all nine responses to within 3e-8.
- Their passbands have near-zero phase relative to the delay. At 0 % their sum is the reference's 0 % response, which
  is not exactly the bypassed path: up to −0.08 dB of ripple at 280 Hz and +2° of phase near 120 Hz.
- The plug-in uses these IRs at 48 kHz. Other rates use a frequency-sampled redesign with the same time delay.

### 3.2 Small-signal gain vs amount

**Method.** Impulse at −60 dBFS, below every threshold.

**Result.** Each band gets a fixed gain. With the crossover above and one real gain per band, the model fits the
measured complex response within ±0.11 dB from 30 Hz to 16 kHz. It is flat at 0 %.

| amount | 0 % | 25 % | 50 % | 75 % | 100 % | 125 % | 150 % | 175 % | 200 % |
|---|---|---|---|---|---|---|---|---|---|
| low dB | 0.00 | 0.74 | 1.47 | 2.16 | 2.83 | 3.17 | 3.49 | 3.81 | 4.11 |
| mid dB | 0.00 | 1.07 | 1.99 | 2.79 | 3.49 | 4.20 | 4.85 | 5.45 | 6.02 |
| high dB | 0.00 | 0.38 | 0.67 | 0.87 | 0.99 | 1.53 | 2.03 | 2.51 | 2.96 |

With the identified FIR bands (3.1) the gains come out slightly different, because they belong to that particular
split: mid 3.60 dB and high 0.91 dB at 100 % (`Source/dsp/BandFilters.h`).

The 0–100 % and 100–200 % halves follow different curves. Above 100 % each band rises by roughly a fixed step per 25 %.

### 3.3 Static curves

**Method.** Sine staircases from −80 to +4 dBFS, 1.2 s per step, read over the last 0.4 s of each step; plots in
`static_<f>hz.png`.

**Result.** Below a band threshold the gain is constant. Above it, the gain falls and crosses 0 dB at about
−11 dBFS (mid and high), then keeps falling. "knee" = the first level where the gain is 0.1 dB under its small-signal
value.

| band (tone) | knee, 50 / 100 / 200 % | gain at −20 / −11 / −2 / +4 dBFS, 100 % |
|---|---|---|
| low (40, 100 Hz) | −17 / −20 / −20…−23 dBFS | 2.80 / 2.09 / −0.20 / −5.1 (100 Hz) |
| mid (300 Hz, 1 kHz) | −32 / −35 / −35 dBFS | 1.85 / 0.02 / −2.77 / −6.3 (1 kHz) |
| crossover region (3 kHz) | −20 / −20 / −23 dBFS | 1.75 / 1.25 / −1.60 / −3.9 |
| high (8, 15 kHz) | −11 / −14 / −17 dBFS, a hard knee | 0.98 / −0.02 / −4.08 / −7.4 (8 kHz) |

The high band behaves like a hard-knee compressor:
- ratio about 1.8 : 1 at 100 % and about 3.7 : 1 at 200 %
- 0.45 and 0.73 dB of gain reduction per dB above the knee

The low and mid bands have a very soft knee spread over 30 dB or more.

### 3.4 Harmonics

**Result.** The character section adds odd harmonics only: H2 and H4 stay at the analysis floor. H3 dominates
(`h3_vs_level.png`).
- At 1 kHz, 100 %, H3 is −56 dBc at −20 dBFS, −41 dBc at −11 and −22 dBc at +1. It rises about 1.7 dB per dB.
- H5 appears above about −8 dBFS.
- The low and mid bands carry the distortion. The 3 kHz H3 is 10–15 dB lower, and the high band adds nothing
  measurable.

So there is a **symmetric (odd) waveshaper** in the low and mid bands.

### 3.5 Time behaviour

**Method.** Bursts at −40 dBFS, stepped to −20 / −6 / 0 dBFS, then back to −40 dBFS. The fundamental's gain is
tracked over time (`envelope_<f>hz.png`).

**Result.** Each band has an instant part (the waveshaper) and a slow part (a compressor):

| band | attack | release | instant part at 0 dBFS |
|---|---|---|---|
| mid (1 kHz) | slow part τ ≈ 100–150 ms | after a +1.9 dB instant jump, an S-shaped recovery: 50 % at ≈ 190 ms, done by 800 ms | ≈ 5.9 of 7.2 dB |
| low (100 Hz) | fast | a fast partial recovery in ~20 ms, then a very slow tail (only 2.0 of 4.0 dB back after 3 s; the GR meter falls ≈ 0.36 dB/s) | most of it |
| high (8 kHz) | ≈ 1–2 ms (63 % at 1.3 ms) | a small instant jump, then linear in dB at ≈ 50 dB/s of detector level. This is a peak detector with τ ≈ 170 ms through a hard knee. | 0.5 to 1.2 dB |

The static staircases show the same thing. Only the low band reads lower when the levels are stepped downwards,
because its slow release has not finished.

### 3.6 Detector and stereo

**Detector.** One sine was compared with two tones of the same RMS (3 dB more peak). The high band reacts to peaks:
it applies 1.0 dB more gain reduction at −10 dBFS RMS. The low and mid bands react partly (0.1–0.6 dB). So none of
the detectors is a plain RMS detector.

**Stereo.** The bands are linked.
- A loud L channel with a quiet probe on R moves the R probe by −3.0 dB.
- L-only input gets the same gain on both channels (the leak into R is at −52 dBFS).
- Detection follows mostly the mid (L+R) signal:
  - L only at −10 dBFS acts like mono at about −15 dBFS.
  - Pure side at −10 dBFS acts like mono at about −30 dBFS.
- Side signals get 0.06 dB more small-signal gain than mid signals.

### 3.7 Side-chain EXT

- Loading a state with `hostOnly.sidechain = 1` takes the main signal out of the character's detectors: −20 dBFS at
  1 kHz is no longer compressed.
- **Not resolved:** audio fed to the side-chain bus through RefRender had no effect, even though the bus was enabled
  in stereo.
- **Next:** check in a DAW (FL Studio side-chain routing).

### 3.8 Per-band structure (Phase 4)

Each band is a **parallel compressor followed by a cubic shaper**:

  u = x · ((1 − m) + m · g(t)),  y = M · (u − c · u³)

- x: the band signal. m: the wet share (0.72 for mid at 100 %). g: the wet path's gain from the compressor.
- c: the shaper (0.95 for mid at 100 %, 1.7 at 200 %). M: the band's small-signal gain.

The evidence:
- For a steady 750 Hz tone, the H3/H1 ratio and the fundamental give the same c at every level (0.96 … 0.82 from
  −30 to +4 dBFS).
- With that c, the wet-path reduction this implies equals the GR meter's reading:
  - 6.85 dB vs 7.1 shown at −6 dBFS
  - 9.4 dB vs 9.9 shown at 0 dBFS

  So **the meters show the wet path's reduction**. That is why they read more than the change you hear.
- Converted to wet-path reduction, the mid band's attack and release look like a mean-square detector with a very
  soft knee: a log-like attack and an S-shaped release.
- The cubic is not clipped: past about +3 dBFS the output folds over (it falls as the input rises), as the reference
  does at 60 Hz.

`fit_character.py` fits m, c, threshold, ratio, knee, attack and release per band and amount node to tone staircases
and bursts. Detectors: mean-square for the low and mid bands, peak for the high band (−47 vs −40 dB residual).

## 4. Limiter (`measure_limiter.py`)

| Property | Measured |
|---|---|
| Gain law | small-signal gain = knob dB − 0.1 dB (0 → −0.10 dB, 5 → +4.90, 12 → +11.90); flat 20 Hz–20 kHz |
| Ceiling | **−0.10 dBFS sample peak** (−0.101 for noise, pink noise and sines at 1–15 kHz, whatever the knob setting) |
| True peak | not limited: inter-sample peaks reach +0.25 dB (9 kHz sine), +0.53 (pink noise) and +2.0 dB (white noise). True-peak (4x) value up to +2.4 dBFS. |
| Look-ahead | 38 samples of delay; the gain starts falling 19 samples before the first loud sample reaches the output |
| Attack ramp | over those 19 samples **1/gain rises linearly**: the detector level is a linear ramp, like a box filter over the look-ahead |
| Fast release | when a short overload ends, part of the gain reduction returns within 8–16 samples |
| Slow release | **(1/gain − 1) decays exponentially with τ = 0.3778 s**: the detector level relaxes to the ceiling. This law holds from 50 ms to 2 s after the end, for 6.6 and 11 dB of reduction. |
| Program dependence | the slow part charges up over the overload (τ ≈ 25 ms on a DC step). After a 5 ms overload it holds 3.4 dB, after 20 ms 7.7 dB, after ≥ 0.3 s all 11 dB. So short peaks release fast and long overloads release slowly. |
| Linking | stereo-linked (a quiet R pilot gets −10.2 dB while L is limited by 10–11 dB) |
| Static | 1 kHz sine: output peak −0.107 dBFS, fundamental −0.093 dBFS (slightly squared waveform) |

**Model for Phase 4.** gain = 1 / max(fast, slow), where:
- fast = a box-smoothed windowed peak over the look-ahead
- slow charges with τ ≈ 25 ms and relaxes to 1 with τ = 0.378 s
- levels are taken relative to the ceiling (0.98855)

## 5. Parameter smoothing (`measure_smoothing.py`)

**Method.** A steady quiet tone, with one parameter jumping at a 64-sample block boundary.

**Result.** Times are from the change at the input (the raw path delay, 3.2 ms with the limiter off and 4.0 ms on,
is included):

| Parameter | 10 % / 50 % / 90 % / 99 % (ms) | Shape |
|---|---|---|
| Input Gain 0 → +12 dB | 5.3 / 9.5 / 22 / 43 | smoothed; longest of all |
| Output Gain 0 → −12 dB | 1.8 / 10.2 / 28 / 50 | smoothed; no path delay (applied at the output) |
| EQ Mid / Low Gain | 4.7 / 7.9 / 11.1 / 12.4 | ≈ 8–10 ms ramp |
| Character 100 → 200 % / → 0 % | 1.5 / 4.8 / 8.1 / 8.9 | ≈ 8 ms ramp, starts before the 3.2 ms path delay |
| Limiter Input Gain 5 → 12 dB | 1.5 / 4.8 / 9.4 / 10.6 | ≈ 9 ms ramp |
| EQ Bypass | switches at 7.0–7.4 | instant, no crossfade |
| Character Bypass | switches at 1.1–1.7 | instant, no crossfade |
| Limiter Bypass | instant | the 38-sample delay changes at once (it can click on program material) |
| Bypass (main) | instant | |

## 6. Meters (`measure_meters.py`, RefCapture film of `reference/gr_meter_test.f32`)

**Band GR meters** (`gr_bars.png`):
- 33 bars, binary (no partial brightness), about **6 dB full scale (≈ 0.18 dB per bar)**.
  - 8 kHz at −6 dBFS: 4.7 dB → 25 bars.
  - 100 Hz at 0 dBFS, low band: 2.8 dB → 15 bars.
  - The mid band is pinned at 33 bars for 7.1 dB and more.
- Once a band has reduced gain, at least one bar stays lit.
- The meter follows the band's internal gain reduction, which is larger than the net change of the audio. For
  example, 1 kHz at −6 dBFS reads 7.1 dB while the fundamental drops 4.9 dB.
- The rise follows the band's attack: the high and low bands reach their bar count within one frame (< 30 ms), and
  the mid band takes ≈ 0.1 s to fill.
- Release follows the band's release:
  - mid: 33 → 1 bars in ≈ 0.5 s after a 0.1–0.4 s hold
  - high: ≈ 0.2 s
  - low: ≈ 1 bar per 0.5 s

**Numeric readouts beside the bars** (band and limiter) **hold the deepest GR so far.**
- mid: −7.1 (1 kHz −6 dBFS), −9.9 (0 dBFS), −13.1 (+6 dBFS)
- high: −4.7
- low: −2.8
- limiter: −0.1, −1.4, −3.0 for the same bursts with default settings

**I/O RMS readout: still open.** Recorded data:
- It is live, not held.
- It reads closer to peak than RMS: a −6 dBFS sine reads −6.9 to −7.2 below 1 kHz, −8.1 at 2–5 kHz and 20 kHz,
  and −7.4 at 8 and 16 kHz.
- Noise at RMS −12 dB reads −10.4.

The short-term LUFS mode and the peak readouts' hold and reset were not measured yet.

## 7. Oversampling (`measure_oversampling.py`)

The setting was switched through the reference's own settings page and restored to 1x afterwards.

| | 1x | 2x | 4x | 8x |
|---|---|---|---|---|
| latency (48 kHz) | 189 | 212 | 217 | 219 |
| bypassed path at 20 / 22 kHz | 0 / 0 dB | −0.53 / −3.3 dB | −0.53 / −3.1 | −0.53 / −3.1 |
| character 200 %, 9 kHz at −3 dBFS: alias at 21 kHz (H3 folded) | −32.7 dBc | −54.2 | −54.3 | −54.3 |
| same, alias at 3 kHz (H5 folded) | −77 dBc | −96 | −111 | −126 |
| character 100 % small signal, 1 kHz at −6 dBFS gain | −1.351 dB | −1.349 | −1.348 | −1.350 |

Oversampling adds only the up/down-sampling filters (a top-octave roll-off) and less aliasing. The dynamics and the
small-signal response below 15 kHz do not change.

**Phase 4.**
- The path's extra delay at 48 kHz, read from the group delay at 1 kHz, is 22.488 / 27.711 / 29.655 samples at
  2x / 4x / 8x.
- That is short, so the reference's filters are short too. Ours are linear-phase FIRs of 45 / 21 / 15 taps per stage.
- The first stage was fitted so that a simulation of the whole path matches the measured response. The path is:
  upsample, then the fixed delay with its fraction at the high rate, then downsample.
- Simulating the whole path matters. Because of the fractional delay between up- and downsampling, the folded image
  does not add in phase. A filter designed as if it did is off by 2 dB at 20 kHz at 2x.
- Result: within 0.002 dB and 0.2° of the reference up to 23.9 kHz at all three ratios (`fit_oversampling.py`).

## 8. What Phase 4 still has to pin down

**Character section** (largest risk for the null tests):
- the exact waveshaper curve and whether it sits before or after the compressors
- each band's attack and release laws and the low band's very slow tail
- the amount mapping of thresholds, ratios and the shaper
- the M/S detector weighting
- the residual ±0.1 dB in the small-signal fit

**Expect these nulls to be the least deep;** each is reported honestly.

**Other items:**
- the limiter's exact fast stage (window and smoothing lengths around the 19-sample ramp)
- the side-chain EXT signal path (needs a DAW check)
- the I/O RMS and LUFS readouts
