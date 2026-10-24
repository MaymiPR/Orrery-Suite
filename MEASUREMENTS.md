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

(The first model; 3.9 replaces the cubic with a sine and 3.10 adds the stereo width.)

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

### 3.9 The shaper is a sine, and the band laws (Phase 4, `nulltest/ident.py`, `fit_bandlaw.py`)

The crossover is known exactly, so for any input the three band signals the reference processes can be computed. A
band's own output is then the reference's output minus the other two bands.

**Shaper.** The output is regressed on powers of the band signal, one cycle at a time. That gives u − c·u³ + 0.3·c²·u⁵
…, the series of a sine. So the shaper is

  y = M · sin(k·u) / k,   u = x · n(t),   n = (1 − mix) + mix · g(t)

- It is memoryless: the harmonics' phases are within 0.1° of the sine's at 300 and 700 Hz.
- It runs at the processing rate without oversampling. A 7 kHz tone aliases the same way in both plug-ins (its
  5th harmonic shows up at 13 kHz, −81 dB).
- A tone in the mid band with left and right in anti-phase is not compressed at all, so it shows the shaper alone.
  On it, k = 2.46 at 100 %, the same at every level. Past k·u = π/2 the output folds over, as a sine does.

**Band laws.** `fit_bandlaw.py` fits each band and amount node on its own:
- the probes are a tone staircase (−40 … +4 dBFS, 3 s steps) and bursts (20 ms, 0.2 s and 2 s at −24 / −12 / 0 dBFS
  over a −50 dBFS bed), at 70 Hz, 700 Hz and 8 kHz
- the free parameters are k, mix, threshold, knee, ratio, attack and release
- the band gain comes from the plug-in's own gain computer (`charsim.dll`, built from BandDynamics.h)
- the other two bands are subtracted through their own fitted laws. This matters: at 70 Hz the mid band still passes
  the tone at −21 dB and compresses it. Treated as linear, the low band fitted to only −31 dB; with the mid band's
  law subtracted it fits to −52 dB.
- the fits have local minima, so `fit_multistart.py` starts each fit from several detector times and from both
  detector types

What the fits show:

| Band | Detector | Attack / release | Static law | Changes with the amount |
|---|---|---|---|---|
| Low | mean-square | ≈ 1 ms / ≈ 2 s | threshold ≈ −10 dB, knee ≈ 28 dB, ratio ≈ 1.3 | k 2.2 … 2.6, mix |
| Mid | mean-square | ≈ 300 ms / 89 ms | threshold ≈ −19.5 dB, knee ≈ 45 dB, ratio ≈ 2.3 | k 0.8 … 3.4, mix 0.27 … 1.0 |
| High | mean-square | ≈ 1 ms / ≈ 91 ms | threshold ≈ −10 dB, hard knee, ratio ≈ 3 | k, mix |

For the mid band only k and the mix change with the amount; the other constants stay put from node to node. Per
band and node the fits reach −50 to −56 dB.

They also hold away from the probe frequency. The mid band's law, fitted at 700 Hz, predicts a 300 Hz staircase to
−55 … −74 dB per step (`predict.py`). So the detectors hear the same FIR band signals as the audio.

### 3.10 Stereo: mid-only detectors and a widened side (Phase 4, `fit_width.py`)

- **Detectors hear the mid only.** A tone in anti-phase (pure side) is not compressed at any level. A left-only
  tone is compressed exactly as a mono tone of half the amplitude (fitted detector scale 0.998).
- **Shaping is per channel.** With a left-only input the right output carries no harmonics.
- **The side is boosted after the shaper.** Low-level noise in the left channel only gives both the mid and the side
  gain of every band: a least-squares fit over the exact band signals leaves −96 dB. The side gain relative to the
  mid, in dB, at amount 0, 25, …, 200 %:

| Band | 0 | 25 | 50 | 75 | 100 | 125 | 150 | 175 | 200 % |
|---|---|---|---|---|---|---|---|---|---|
| Low | 0 | 0.000 | −0.001 | −0.001 | −0.001 | −0.001 | −0.004 | −0.008 | −0.012 |
| Mid | 0 | 0.004 | 0.008 | 0.011 | 0.014 | 0.015 | 0.339 | 0.801 | 1.240 |
| High | 0 | 0.895 | 1.711 | 2.462 | 3.157 | 3.549 | 3.740 | 3.836 | 3.929 |

  So at 100 % a left-only signal above 5 kHz leaks into the right channel at −15 dB. The boost comes after the
  shaper: a left-only tone at 200 % fits to −45 dB that way and to −32 dB with the boost before it.

Modelling the stereo width and the band laws took the character-only null on the music test signal from −40 to
−63 dB at 100 %, and from −36 to −62 dB at 200 %.

### 3.11 What the nonlinear stages hear (Phase 4, `nulltest/nlhear.py`)

The FIR bands have shallow stopbands. The mid band passes 8 kHz at −29.5 dB and stays at −28 dB up to 20 kHz. This
is the complementary remainder of a split whose high band sits 0.3 dB low. Part of it bypasses the bands'
nonlinear stages.

**The probe.** A quiet tone in the band (−30 dBFS) runs under a loud interferer (−2 dBFS) stepped across frequency.
The interferer changes the probe's gain through the band's detector and through cross-modulation in its shaper. For
each step, the interferer level at the nonlinear stage's input that reproduces the reference's probe gain is solved
for. The other bands run through their own laws, since they pass a little of the probe and the interferer may
compress them.

Mid band (probe at 700 Hz), in dB re the interferer:

| Interferer | 30 | 70 | 130 | 200 Hz | 1.6 | 2.7 | 3.5 | 5 | 6.5 | 8 | 10–16 kHz |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Nonlinear stage hears | −26.6 | −23.0 | −11.0 | −3.2 | −1.4 | −7.4 | −13.8 | −25.4 | −34.4 | −40.5 | ≈ −43 (the limit of the method) |
| FIR band passes | −25.5 | −22.0 | −10.5 | −3.1 | −1.3 | −7.0 | −12.6 | −21.8 | −29.9 | −29.5 | −28 |

So on its low side the mid stage hears its FIR band, but above the 2.68 kHz crossover it hears a steep
Linkwitz-Riley slope. A model whose mid stage heard the whole FIR band compressed the probe 0.6 dB too much under a
loud 8 kHz tone; the reference changes it by only 0.06 dB. The low band's stage hears its FIR band as far as this
probe can tell (to about 300 Hz).

**Model.** Each band's nonlinear stage works on s_b: the FIR band with Linkwitz-Riley edges beyond its band
(NlBandFilters.h, `ident.nl_firs`; the low band uses its FIR band as it is). The rest of the band passes linearly:

  y_b = M_b · (x_b − s_b + sin(k · n(s_b) · s_b) / k)

The small-signal response is unchanged and still exact.

A steep low edge for the high band's stage, by symmetry, made the broadband nulls worse. A steep edge for the low
band would need more pre-ringing than the 151 samples the delay allows. So only the mid band's upper edge is steep.

### 3.11b The amount fades the shaper in (Phase 4, `fit_bandlaw.py --shapedry`, `fit_music.py`)

Below 100 % the shaper is not driven more gently. A fully driven shaper is blended with the dry band:

  y = d · u + (1 − d) · sin(k · u) / k

The evidence:
- At 50 % the low band's 5th harmonic is far above what a sine of the fitted drive makes (−41 against −49 dBc at
  +4 dBFS). Compared with the 3rd harmonic squared it is 0.62–0.73, where a pure sine gives 0.3.
- With d free, the probe fits jump: low band at 25 % from −42 to −64 dB, mid at 25 % from −52 to −88 dB.
- d comes out as 1 − amount / 100 % in both bands (low 0.73 / 0.47 / 0.23 / 0, mid 0.76 / 0.50 / 0.22 / 0 at 25 /
  50 / 75 / 100 %). The drive rises only slowly below 100 % and faster above.

**Fitted on program material.** Fits to the probes alone have near-equivalent minima that differ on music.
`fit_music.py` therefore fits each band and node against the reference on music, drums, a bass-heavy mix, pink
noise and the probes at once. The whole section is modelled in Python (`charmodel.py`, identical to the plug-in to
−120 dB). Every start then converges to the same law, and the laws come out the same at every amount:

| Band | Threshold | Knee | Ratio | Attack / release | What moves with the amount |
|---|---|---|---|---|---|
| Low | −9.4 dB | 29.7 dB | 1.33 | 1.0 ms / 2.0 s | drive 1.89 … 2.59, mix 0.19 … 1.0, d |
| Mid | −19 … −20 dB | 44 … 47 dB | 2.3 … 2.7 | 310 … 325 ms / 86 … 88 ms | drive 1.75 … 3.42, mix 0.25 … 1.0, d |

Between the nodes the laws interpolate well: 37.5 % nulls at −71 dB that way, against −34 dB with laws fitted
directly at the half nodes. On music the character section now nulls to −70 dB from 12.5 to 162.5 %.

The same approach did not work for the high band at first: its music-fitted laws nulled music as well, but drums
5 dB worse. The reason was its shaper, which no fit had right (3.14).

### 3.12 Other sample rates (Phase 4, `nulltest/ident_rate.py`)

At 44.1, 88.2, 96 and 192 kHz the nine small-signal impulse responses again span exactly three filters (rank-3 fit
error below 1e-7), with the same per-band gains as at 48 kHz.

- The reference does not resample its 48 kHz filters. It designs them for each rate: 608 taps at 44.1 kHz, 1216 at
  88.2, 1323 at 96 and 2647 at 192 kHz.
- At 88.2 kHz and above all bands fall away above about 22.5 kHz.
- Our frequency-sampled redesign of the 48 kHz filters was off by −23 to −57 dB per band. The worst was the high band
  near the top of the spectrum.
- Ours now uses the reference's own filters at those rates (BandFiltersRates.h), to −89 … −123 dB. With
  oversampling the redesign is still used.

### 3.13 The high band's detector (Phase 4, `nulltest/crest.py`)

A sine, two tones and band noise, all at the same RMS, through each band:

| | Mean-square detector | Peak-sensitive detector |
|---|---|---|
| Prediction | the same gain for all three | more compression for peakier signals |
| Mid band | **observed:** ours matches within 0.02 dB | |
| High band | | **observed:** at 100 % and −10 dB, −1.50 (sine), −2.52 (two tones), −3.24 dB (noise) |

So the high band's detector reacts to peaks. A single tone cannot tell the two kinds apart, which is why the laws
fitted to an 8 kHz tone missed on broadband material. The broadband fit has a fast peak detector (about 1.2 ms /
175 ms) and a slow one about 19 dB down. It gets this pattern right but compresses 3–4 % too little.

It reads the **sample peaks**, not the true peak (`nulltest/staticn.py`, read through the measured shaper of 3.14). A
steady 12 kHz tone is 4 samples per cycle, so its sampled peak depends on its phase while its true peak does not. At
−9 dBFS the reference reduces it by 1.41 dB at one phase and 1.63 dB at the other (0.89 and 0.93 of the amplitude
sampled); ours, with a sample-peak detector, by 1.34 and 1.55 dB.

### 3.14 The shapers read directly (Phase 4, `nulltest/shapeside.py`, `h3cmp.py`, `shapemid.py`)

A tone with left and right in anti-phase is not compressed (the detectors hear the mid only, 3.10), so a band's side
output is its shaper alone: plotted sample by sample against the band signal, it is the shaper's curve. The tones
sit where our own plug-in reads back its own settings exactly: 9 kHz (high band), 800 Hz (mid), 25 Hz (low).

| | Pure sine, sin(k·u)/k | With a dry share, d·u + (1 − d)·sin(k·u)/k |
|---|---|---|
| High band, reference (fit residual) | −46 … −57 dB | **−92 … −124 dB** at every amount |

- **High band.** k rises linearly with the amount: 1.40 at 12.5 %, 1.865 at 100 %, then more slowly to 2.34 at 200 %.
  d falls smoothly from 0.89 at 12.5 % to −0.037 at 100 % and stays near −0.04 above. The fitted laws had d = 0 and
  k off by up to 8 %, which the compressor fits had been compensating.
- **Mid and low bands.** Below 100 % the music-fitted laws already match (k within 1–2 %, d within 0.01). Above 100 %
  the reference's mid k reads 3–4 % lower than ours, with d = −0.01. Refitting the mid band with those values made
  music at 200 % worse (−62.3 → −61.2 dB), so the mid and low bands keep their fitted laws.

**Mid and side are shaped differently.** Below the threshold, a high-band tone in phase has a 3rd harmonic 1.53 %
stronger than the same tone in anti-phase (100 %), at every level from −30 to −15 dBFS, with the same fundamental.
So the gain is the same and the shaper is not. A tone in the left channel only splits the difference exactly: the mid
part of its harmonic is the in-phase strength, the side part the anti-phase one. So the shaper's distortion,
(1 − d)·(sin(k·u)/k − u), reaches the output's mid 1 + γ times as strong as its side. γ grows by 0.0019 per 12.5 %
step up to 0.0153 at 100 %, peaks at 0.0177 at 137.5 % and is 0.0142 at 200 %. In the mid band the reference's
in/anti ratio matches ours to 0.1–0.4 %. Ours now does the same (BandParam `shapeMid`, from `shapemid.py`).

With both, treble-heavy material below the threshold ("bright −8") nulls at −110.8 dB, against −76 dB before.

**The high band's compressor, refitted around the measured shaper** (`fit_music.py --shaper --loud`). With the
shaper fixed, only the gain computer is fitted, and the program set now includes loud material where the high band
works hardest (drums peaking at −1 dBFS, pink noise at −6 dBFS RMS, music at 0 dBFS, bright −8). The trace of the
band's gain on loud drums (`hightrace2.py`) had shown the old law starting to compress late and holding about 0.6 dB
too little through every hit. At 100 % the law fitted with each signal weighed equally in dB (`--relative`) keeps
pink −6 and bright −8 best; at the other amounts the plain energy objective is better.

| Case (stress suite) | Before | After |
|---|---|---|
| Full chain, drums peaking at −1 dBFS | −34.7 dB | −44.7 dB |
| Character only, pink −10, 25 / 62.5 / 150 / 200 % | −48.2 / −43.5 / −42.3 / −34.8 | −58.7 / −53.5 / −45.6 / −42.7 |
| EQ + character, 200 %, pink −10 | −38.2 | −42.2 |
| Bright −8, defaults | −76.1 | −110.8 |

The gain law itself, read through the shaper on steady tones (`staticn.py`), still differs a little: the reference
has a sharper knee and compresses 0.08–0.24 dB more above it (at the same sampled peak), which our parallel soft-knee
form cannot follow (it fits the reference's curve to 0.027 dB rms, ours to 0.001).

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

### 4.1 The algorithm, identified with probes (Phase 4, `nulltest/lim_probe.py`)

Isolated clicks, click pairs and trains at chosen spacings and heights, with the gain trace read back sample by
sample. The model below reproduces the probes to 0.0007 in gain and the trains to an RMS error of 0.00035.

Every level is in units of the 0 dBFS ceiling, and never less than 1. The gain is `trim / env`, with trim = −0.1 dB.
env is the largest of three things:
- **The level of the outgoing sample.** This is its inter-sample peak over [n, n + 1). It comes from a 16-phase
  interpolator: a Kaiser-windowed sinc with 16 taps. So the ceiling holds on the samples, not on the true peak.
- **A straight-line plan through the coming peaks.**
  - Only local maxima of the level are targets.
  - When a plan ends, the next one heads for the nearest peak within 21 samples ahead (at 48 kHz) and reaches it
    exactly on arrival. A farther, higher peak does not steepen the line.
  - With no peak ahead, the plan falls toward 1 over 22 samples. It stops at the slow envelope if it meets it.
    Right after a peak it falls instead toward the level 22.9 samples ahead (interpolated at that point; the flank of
    a peak still beyond the window), so it meets the next rise instead of overshooting below it.
  - A running plan toward a peak changes only when a level within its span rises above its line. It then re-plans
    to the steepest such point.
  - A release watches the whole 21-sample window, with its line continued at 1 past its end. So a second peak 24 to
    40 samples after the first ends the release as soon as it comes within 21 samples. The release then turns into a
    straight line up (or down) to that peak, instead of dropping toward 1 and jumping back. This one rule took the
    drums limiter null from −45.8 to −62.5 dB.
  - An idle limiter (no plan, state at or under the slow envelope) rests on the slow envelope.
- **A slow envelope.** It charges toward the level with τ = 24.47 ms. When the level is lower, it relaxes toward the
  current level (1 when quiet) with τ = 377.8 ms.

Limiter-only nulls are −47.5 (pink noise, gain 5), −62.5 (drums, gain 12) and −60.8 dB (music, gain 5). A pair of
peaks exactly 24 samples apart still differs for the first 3 samples after the first peak.

**Per rate.** The window and the release are not scaled from 48 kHz. Probed at each rate:

| Rate (kHz) | 44.1 | 48 | 88.2 | 96 | 192 |
|---|---|---|---|---|---|
| Look-ahead | 40 | 38 | 50 | 54 | 96 |
| Window | 19 | 21 | 39 | 43 | 86 |
| Release (samples) | 20 | 22 | 40 | 44 | 87 |

With these the single-peak and pair probes match to 0.004 at 96 and 192 kHz. Pairs exactly one sample beyond the
window still differ at 44.1 and 88.2 kHz: after a peak the reference seems to look one sample further than its
attack from idle.

At 96 kHz one recurring event in a drum loop nulls to only −35 dB. There, a 1e-4 DC pilot changes the reference's
output by orders of magnitude more than its size, so its peak planning sits on a tie. Matching such ties would need
the reference's inter-sample interpolator bit-exact.

**The level estimate, read off isolated pulses** (`nulltest/limpulse.py`, `limident.py`, `limpy.py`). An isolated
pulse above the ceiling is planned to exactly its level, so at its loudest point output / input = trim / level. With
4096 random pulses:
- On the samples themselves the reference reads 1.00014 times the sample (the fitted phase-0 filter is a pure delay
  with that gain): an effective ceiling 0.0012 dB lower. Ours now does the same (`limLevelScale`). The music limiter
  null went from −60.8 to −62.3 dB.
- Between the samples, a free 16 × 16 table fitted to band-limited pulses reads them to 5e-5 (ours: 7e-4) and takes
  the drum null to −66.9 dB. But it does not carry over to content near Nyquist: pink noise gets worse (−47.5 →
  −44.6 dB), and on short raw noise bursts the reference's readings differ from any 16- or 32-tap linear table by up
  to 2–3 %. So the interpolator stays the Kaiser sinc.
- The plan rule itself is right: in a Python copy of the limiter that matches the plug-in exactly, every variant
  (peaks of the sample magnitudes, peaks over two neighbours, a minimum prominence, the steepest reachable peak) was
  far worse (pink noise −21 … −29 dB against −47.5). The pink-noise events that differ are near-ties: a local maximum
  0.001 above its neighbour that the reference does not see as one.

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

### 5.1 The control rate (Phase 4, `nulltest/steps.py`, per-sample envelopes of steady tones)

The reference moves its parameters at a control rate.

**Chunks.** It cuts each host block into chunks of up to 128 samples, counted from the block's start. With 64-sample
blocks the chunks are 64 samples long.

**The input and output gains** are one-pole smoothed once per chunk, by chunk/τ of the remaining distance in linear gain
(forward Euler, τ = 478.8 samples, about 10 ms). The gain ramps linearly between chunk ends: the slope of the output
is piecewise constant, in 128-sample pieces.

The input gain has a second stage in front: the parameter itself ramps linearly in dB over 237.6 samples (about 5 ms),
and its value is read at each chunk's end. So in the first chunk after a change the target is only about halfway
there. The output gain has no such ramp. This model reproduces the reference's input-gain steps at block sizes 64
and 128 for +12, +6 and −12 dB to within 0.0005 in gain.

**The EQ gains and the amount** go through the same 5 ms parameter ramp, read at chunk ends. Then they glide linearly
over 5 ms to each new value; the glide is re-planned only when the value changes. For a step that means a slower
first chunk, then a straight line to the end, about 368 samples after the change. That is the shape the reference's
EQ shows. Its EQ filters are state-variable filters (their response while gliding matches; direct forms do not),
and their gain glides in linear amplitude, not in dB.

**The limiter's input gain** ramps linearly over 10 ms in linear gain, with no chunking (to within 0.003 dB).

**Switches.**
- The EQ switch follows its own 5 ms ramp and takes effect at the start of the chunk after the one in which the ramp
  crossed half way. With 512-sample blocks that is 128 samples after the block start.
- The character section and the limiter switch at once. While switched off they do not run; switched back on, they
  start from silence (detectors at rest, empty filter and look-ahead buffers). Modelling the stages as kept running
  left the bypass toggles at −31 dB; this way they null at −60 dB.
- At amount 0 the character section's detectors keep running. A jump from 0 % settles as fast as one from 100 %.

**Host blocks shorter than 128 samples.** There the switches of the oversampled section (EQ and character on/off)
take effect once a control chunk's worth has gathered: 128 − n rounded up to whole blocks later (96 samples at
32-sample blocks, 64 at 64, 96 at 96). Ours does the same; the bypass toggles at 64-sample blocks went from −33 to
−57.8 dB, and every stress case at 64, 333 and 4096-sample blocks now nulls as at 512.

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
- The reference's **8x output equals its 4x output, two samples later** (−82 dB below 16 kHz). So ours processes 8x
  at 4x.
- With oversampling the reference's character path keeps its ~22.5 kHz low-pass and sits 0.012 / 0.040 samples
  later than its bypassed path (2x / 4x). Ours does the same.

## 8. Still open after Phase 4

How closely the result matches is in [NULLTESTS.md](NULLTESTS.md).

**Settled in Phase 4:**
- the crossover: FIR, identified exactly (3.1)
- the shaper: a sine, sin(k·u) / k, after the parallel mix (3.9), blended with the dry band (3.11b, 3.14)
- the stereo behaviour: mid-only detectors, per-channel shaping, a side gain per band after the shaper (3.10)
- the limiter's algorithm (4.1)
- the 8x setting: it behaves exactly like 4x, two samples later. Its output equals the 4x output to −82 dB below 16 kHz.
- the oversampled path: matched within 0.002 dB (7)

Also settled since: the shapers' exact form, a dry share and a mid/side difference (3.14); the high band's sample-peak
detector (3.13); host blocks under 128 samples (5.1: the switches wait for a full control chunk; 64-sample blocks
null like 512); the limiter's on-sample level (4.1).

**Still open:**
- **The high band's gain law.** Its knee is sharper and its slope steeper than our parallel soft-knee form allows
  (3.14), and its probes (tones up to +4 dBFS, band noise) still null only to −25 … −45 dB at 100 % and above. Loud
  dense material is the weakest case: pink noise at −6 dBFS RMS −40.6 dB, drums peaking at −1 dBFS −44.7 dB, and at
  44.1 kHz −38.6 / −39.5 dB (the laws are fitted at 48 kHz).
- **Limiter.** Dense, continuous limiting (pink noise driven 5 dB into it) nulls to −47.5 dB; isolated peaks and
  drum hits match to −62 dB. The differences are near-ties in the inter-sample level; the reference's interpolator
  is not a linear 16- or 32-tap table near Nyquist (4.1). At 96 kHz one drum event sits on such a tie.
- **The sweep's crossover region.** Between 1.5 and 3.5 kHz ours compresses up to 0.1 dB more (sweep at −6 dBFS:
  −45 dB). That is where the mid band's steep edge and the high band meet.
- **Side-chain EXT.** Audio fed to the reference's side-chain bus had no effect in our test host. Ours is implemented
  as described (the side-chain drives the band detectors) but could not be compared.
- **Meters.** The I/O RMS and LUFS readouts and the limiter's dash meter (step size estimated) are not measured yet.
