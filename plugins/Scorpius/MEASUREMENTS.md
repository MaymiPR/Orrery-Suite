# Measurements (Phase 3)

What the reference, **Cradle Orion 1.2.1** (VST3, Windows), does to audio. Everything here was measured by
rendering test signals through it offline. The DSP was not decompiled, and Cradle's encrypted scripts and its neural
model were not opened. The numbers are the targets for Scorpius's DSP (Phase 4).

**Data stays local.** Data and plots live in `measurements/scorpius/` and the renders in `nulltest/renders/`. Both
folders are git-ignored by the project rule, so this file is the published record. Every number below can be
reproduced with the scripts named in each section.

## How it was measured

| Piece | What it does |
|---|---|
| `tools/RefRender.cpp` | Offline JUCE VST3 host (shared with The Antiparticle). Renders float32 files through a plug-in, sets parameters by displayed value, loads a saved state. New for Orion: `settle=ms` runs the message loop after the parameters are set (see below). |
| `nulltest/orion.py` | Python harness: `render_many(jobs)`, `ONLY(...)` (only the named modules on), hidden settings written into a copy of the default state (`hidden={'drive.driveMode': 'b'}`, `{'ids.compression.mode': 'Mode A'}`), test signals, nulls, an on-disk render cache. |
| `nulltest/orionstate.py` | Edits a saved state by its path in the preset table (the hidden settings and `prefs`). |
| `nulltest/survey.py`, `order.py`, `linear.py` | The first look at every module, the signal order, and the small-signal impulse responses (`measurements/scorpius/ir/`). |

**Conventions:**
- 48 kHz, block 512. The plug-in opens with the factory preset "start me up"; each test switches the other modules
  off with their own power buttons (`ONLY`).
- "Gain" means the level of the test tone's fundamental (Hann window) at the output re the input.
- Nulls leave out the first 0.1 s of a render (see the ramp below).

**Pitfalls found on the way:**
- **Settings apply asynchronously.** Parameters set before a render take effect partway through it, at a random
  point (the first renders were not repeatable: two renders of one input nulled at −8 dB). Running the message loop
  for 400 ms after setting them makes renders bit-identical.
- **Settings ramp in.** Over the first ~5 ms of a render the processing differs (a ramp from the preset's values).
  After it, chaining modules through separate renders reproduces them to −85 … −115 dB.
- **A noise floor.** A time-shifted input leaves a residual of −117 dBFS whatever the level: the reference adds a
  very low noise (or dither) while a module is on. It limits nulls on quiet signals only.

## 1. Structure (`order.py`)

Both modules on, against the two modules rendered one after the other in each order (nulls in dB):

| Pair | This order | The other order |
|---|---|---|
| drive, harmonics | drive → harmonics −115 | −16 |
| drive, compression | drive → compression −106 | −16 |
| harmonics, compression | harmonics → compression −106 | −10 |
| harmonics, lift | harmonics → lift −87 | −13 |
| compression, lift | compression → lift −85 | −20 |
| any module, EQ | module → EQ −287 … −291 | −12 … −20 |

**The chain:** input gain → **drive** (+ hi-pass) → **harmonics** → **compression** → **lift** → **harmonic EQ** →
the **parallels** → output gain.

**The parallels** (sub, low, loud, grit) are fed from the end of the chain (after the EQ): each nulls exactly
(−284 … −294 dB) against "chain, then the parallel" (one exception: grit, below). Two parallels on at once equal the
sum of their own contributions, **out = chain + Σ (pathₖ − chain)**, to −143 … −145 dB: they are independent paths
mixed in, not a series.

**Grit** is the exception: after drive (−86 dB) and after compression (−31 dB) it does not null against "chain, then
grit". Its detector seems to listen to something inside the chain (Section 9, to come).

**All modules off:** bit-exact (null −280 dB at 48 kHz), latency 0.

## 2. Sample rates (resampling)

The DSP runs at a fixed 48 kHz (as the binary's `ResampledAudioGraph<FixedSampleRatePolicy>` suggests). At other
rates the audio is resampled in and out with a linear-phase low-pass (it rings before the impulse):

| Rate | Reported latency | Impulse peak | All modules off: null vs input | Response near the top |
|---|---|---|---|---|
| 44.1 kHz | 31 | 31 | −38 dB | −0.07 dB at 20 kHz, −0.3 at 22 kHz |
| 48 kHz | 0 | 0 | bit-exact | flat |
| 88.2 kHz | 46 | 49 | (band-limited) | −0.65 dB at 20 kHz, −4.4 at 22 kHz, −83 at 30 kHz |
| 96 kHz | 48 | 52 | (band-limited) | −0.74 / −4.2 / −104 dB |
| 192 kHz | 80 | 88 | (band-limited) | −1.0 / −3.9 / −83 dB |

At high rates the output stops at ~23 kHz (the 48 kHz processing band). The reported latency is 3–8 samples short of
the impulse's peak at 88.2 / 96 / 192 kHz.

## 3. The low-level expander

Drive, harmonics, compression and lift share **one downward expander** (the EQ and the parallels have none; with all
four off there is none):
- Static: a 1 kHz tone passes unchanged down to about **−56 dBFS peak**; below, the output falls **3 dB per dB**
  (−59 dBFS: −1.6 dB; −65: −13.8; −71: −26.0; −80: −44.4).
- It reads the peak, not the RMS: white noise at −62 dBFS RMS (peaks −50) passes unchanged.
- It opens at once (within one block) and **holds for about 1.2 s** after the level drops, then closes over about
  0.6 s; the same from −20 or −40 dBFS.

## 4. Small-signal responses (`linear.py`)

Impulse responses from 20 s of white noise at −50 dBFS (above the expander, below the modules' saturation). Gain in
dB at chosen frequencies; every module processes both channels alike (L − R below −300 dB):

| Case | 20 | 30 | 40 | 50 | 60 | 80 | 100 | 150 | 200 | 500 | 1k | 2k | 5k | 10k | 15k | 20k |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| drive a | −33.6 | −13.5 | −4.1 | +0.5 | −1.8 | −0.9 | −0.3 | −0.5 | −0.8 | −1.3 | −1.4 | −1.9 | −1.7 | −0.7 | +0.4 | +1.9 |
| drive b | −22.1 | −11.2 | −6.0 | +0.9 | −0.4 | −0.4 | 0.0 | −0.5 | −0.8 | −0.8 | −0.8 | −1.2 | −0.9 | +0.8 | +2.4 | +3.2 |
| drive c | −14.4 | −10.3 | −4.7 | +2.7 | +1.5 | +1.4 | +1.9 | +1.5 | +1.2 | +1.3 | +1.3 | +0.8 | +1.2 | +2.5 | +3.5 | +3.9 |
| harmonics 80 Hz, 1.0 | −2.2 | −1.2 | +1.3 | +3.0 | +4.0 | +5.1 | +5.6 | +5.7 | +5.2 | +2.2 | +0.7 | +0.2 | 0 | 0 | 0 | 0 |
| harmonics 50 Hz, 1.0 | −3.0 | +0.7 | +3.4 | +4.7 | +5.3 | +5.7 | +5.6 | +4.8 | +3.8 | +1.1 | +0.3 | +0.1 | 0 | 0 | 0 | 0 |
| compression b, 1.0 | +2.4 | +3.1 | +3.4 | +3.5 | +3.6 | +3.6 | +3.7 | +3.7 | +3.7 | +3.7 | +3.7 | +3.7 | +3.7 | +3.6 | +3.3 | +3.1 |
| compression a, 1.0 | +2.1 | +2.8 | +3.1 | +3.2 | +3.3 | +3.3 | +3.4 | +3.4 | +3.4 | +3.4 | +3.4 | +3.4 | +3.3 | +3.3 | +3.1 | +3.0 |
| lift 1.0 | +2.5 | +3.0 | +3.2 | +3.3 | +3.3 | +3.4 | +3.5 | +3.5 | +3.5 | +3.8 | +4.3 | +4.2 | +4.1 | +3.5 | +2.4 | +1.5 |
| lift 0.0 | +0.5 | +0.5 | +0.4 | +0.3 | +0.3 | +0.3 | +0.3 | +0.2 | +0.2 | +0.3 | +0.5 | +0.2 | 0 | 0 | 0 | 0 |
| EQ 40 Hz +6 | +4.4 | +6.3 | +5.5 | +4.1 | +3.0 | +1.7 | +1.0 | +0.4 | +0.2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| EQ 100 Hz +6 | −1.7 | −2.0 | −0.2 | +1.7 | +3.5 | +6.0 | +7.0 | +5.6 | +3.8 | +0.8 | +0.2 | +0.1 | 0 | 0 | 0 | 0 |
| EQ 1 kHz +6 | −0.2 | −0.3 | −0.3 | −0.3 | −0.3 | −0.2 | −0.1 | +0.1 | +0.5 | +3.5 | +7.1 | +3.5 | +0.6 | +0.1 | 0 | 0 |
| EQ 3 kHz +6 | 0 | −0.1 | −0.1 | −0.1 | −0.1 | −0.1 | −0.1 | −0.1 | 0 | +0.4 | +1.7 | +5.3 | +4.6 | +1.1 | +0.3 | +0.1 |
| EQ 10 kHz +6 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | +0.4 | +3.9 | +6.7 | +7.1 |
| sub 1.0 | +1.6 | +8.3 | +9.4 | +9.4 | +9.0 | +7.0 | +3.0 | +3.6 | +1.9 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| low 1.0 | −0.8 | +1.5 | +0.6 | +0.6 | +2.6 | −0.2 | −0.5 | −0.1 | −0.1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| loud 1.0 | +4.4 | +4.3 | +4.1 | +4.1 | +4.1 | +4.2 | +4.3 | +4.6 | +4.5 | +4.6 | +4.6 | +4.7 | +4.5 | +4.7 | +4.6 | +4.7 |
| grit 1.0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

- The drive's hi-pass switch changes almost nothing (below 20 Hz only: −33.6 → −34.4 dB at 20 Hz for a; its main
  low cut is the drive's own, below).
- A "+6" EQ band peaks at +7.1 dB; 3 dB gives +3.7. The 40 Hz band peaks at ~30 Hz; the 10 kHz band is a shelf.
  The 100 Hz band cuts the deep lows a little (−2 dB at 30 Hz). Sub has a notch near 125 Hz.
- Compression's and lift's small-signal gain is the make-up of their wet path (Section 5).

## 5. Amount laws

Each amount between 0 and 1 against the best mix of the renders at 0 and 1, y ≈ y₀ + w (y₁ − y₀) (drum-like test
signal; nulls in dB):

| Control | w at 0.25 / 0.5 / 0.75 | Null |
|---|---|---|
| compression | 0.250 / 0.500 / 0.750 | −137 … −145: an exact wet / dry mix |
| lift | 0.250 / 0.500 / 0.750 | −137: an exact mix |
| harmonics | 0.237 / 0.483 / 0.738 | −48 … −51: almost a mix |
| sub, low, loud levels | 0.092 / 0.251 / 0.526 | −139 … −156: an exact mix, with the faders' taper (e^(2.188 L) − 1) / (e^2.188 − 1) (UI round 2) |
| grit level | 0.088 / 0.241 / 0.505 | −36 … −51 here; later probes (Section 14) find grit's contribution an exact mix too (−100 … −126 dB re the contribution at 0.088 / 0.5 / 0.731 on drums and the track), except once (drums at 0.189: −10) |

## 6. Drive

- **No oversampling:** a 9 kHz tone at 0 dBFS aliases at −26 … −36 dB (lift aliases too).
- **An asymmetric saturator, then a low cut.** A 1 kHz tone flattens at **+0.49 / −0.43** (colour a), **+0.63 /
  −0.53** (b), **+0.85 / −0.70** (c) (the asymmetry is the second harmonic, present even at −50 dBFS: −84 dB for a,
  rising 10 dB per 10 dB). A 25 Hz tone at 0 dBFS still clips fully (output peaks as at 1 kHz) and its second
  harmonic ends up above the fundamental (+2 dB): the fundamental is cut **after** the saturator, by the steep low
  cut of Section 4 (−34 dB at 20 Hz, a resonant peak near 50 Hz).
- Gain of a 1 kHz tone: a −1.4 dB up to −12 dBFS, −5.2 at 0, −10.9 at +6; b −0.8 / −4.1 / −9.2; c +1.2 / −1.7 / −6.9.
- The loops of output against input are wide (filters with phase around the saturator); the full model is fitted in
  Phase 4.

## 7. Harmonics (the reference's neural model, `Harmonics - FULL WET … LSTM`)

- Small signal: a bass boost, +5.7 dB around 100 … 125 Hz at 80 Hz (around 80 … 100 Hz at 50 Hz), Section 4.
- **Odd harmonics only, as from a cubic term:** h3 rises exactly 2 dB per dB (100 Hz at amount 1: −97 dB at
  −30 dBFS, −74 at −18, −62 at −12, −50 at −6, −39 at 0), h5 4 dB per dB (−66 at 0 dBFS). Nearly the same from 30
  to 400 Hz (−39 … −45 dB at 0 dBFS), much less at 1 kHz (−64): the shaper works on the lows.
- Little compression: the boost at 120 Hz is +5.8 dB up to −12 dBFS, +5.3 at 0, +4.3 at +6.
- The amount is almost a wet / dry mix (Section 5).
- A filter → odd saturator → filter structure looks close enough to try before training a model of our own (Phase 4).

## 8. Compression (`comp.py`)

Amount 1 (the wet path; the amount mixes it with the dry signal exactly):
- **Static curve** (1 kHz): make-up **+3.4 dB (mode a), +3.7 dB (mode b)** below −16 dBFS; a soft knee from about
  −14 dBFS; above it **0.8 dB of gain reduction per dB (ratio 5:1)**: −8 dBFS 0.0 / +0.3, −6 −1.6 / −1.3, 0 −6.4 /
  −6.1, +12 −16.0 / −15.7 dB (a / b). The curves of the two modes are the same but for the make-up.
- **Detector:** the same at every frequency (30 Hz … 15 kHz: 4.9 … 5.0 dB of reduction at −6 dBFS); a fast envelope
  between peak and RMS (noise of the tone's RMS is reduced 4.1 dB more than the tone, noise of the tone's peak 1.7 dB
  less); **stereo-linked on the mid signal** (one channel at −6 dBFS reduces the other, quiet one, by 0.9 dB, as
  (L + R) / 2 at −12 dBFS would).
- **Fast:** after a step from −30 to +6 dBFS the reduction reaches 90 % in ~2 ms (−9 dB of −11 after 2 ms); it
  recovers in ~20 ms (the make-up back after 30 … 40 ms). Gain modulation within the cycle gives odd harmonics at
  low frequencies (40 Hz at 0 dBFS: h3 −27 dB).
- **The modes** agree in every tone test (static, steps, bursts, low frequencies) but for the 0.33 dB of make-up,
  yet on drums they differ (null −23 dB after the make-up): a difference in the fine dynamics, to be found by fitting
  both to program material (Phase 4).
- **The wet path is not linear besides its gain** (`comp_colour.py`): steady tones below the threshold, where the gain
  stands still, come out with even harmonics in both modes: h2 −88 dB re the tone at −36 dBFS, −70 at −24, −65 at
  −18 (1 kHz; 100 Hz alike), about 1 dB more per dB, as from a gentle quadratic term; with compression's amount at 0
  nothing (below −150 dB). The reference's binary names two 1-layer LSTM captures for a compressor's two colours
  "with no compression" (the user allowed a behaviour-only look at it, 2026-10-28; notes kept locally): modes a and b
  very likely colour the wet signal with a model each, after the gain (an inference).

## 9. Lift

- Amount 0 is not the dry signal: a fixed gentle lift, +0.3 dB at 100 Hz, +0.5 at 1 kHz, 0 above 5 kHz (linear).
- Amount 1: a saturator with both even and odd harmonics (1 kHz: h2 −48 / h3 −69 dB at −30 dBFS, h2 −28 / h3 −12
  at 0 dBFS) between fixed filters: the gain curve has the same shape at 100 Hz, 1, 5 and 10 kHz (+3.5 / +4.3 /
  +4.1 / +3.5 dB small-signal, about 7 dB less at 0 dBFS).
- No oversampling (aliases at −18 … −44 dB from a 9 kHz tone at 0 dBFS).

## 10. Harmonic EQ

- Linear below about −20 dBFS (Section 4 for the band shapes).
- Each boosted band **saturates symmetrically**: odd harmonics (1 kHz +6 dB: h3 −88 dB at −40 dBFS, −48 at −20,
  −22 at 0) and its boost shrinks (+7.1 → +6.2 at −10 dBFS, +3.6 at 0). The 10 kHz shelf compresses too (+3.9 →
  +2.0 dB at 0 dBFS) with its harmonics above the band.

## 11. Parallels

Each path is mixed in with the faders' taper (Section 5); levels below at level 1.
- **Sub:** linear at every level (+9.5 dB at 50 Hz from −40 to 0 dBFS); the response of Section 4 (a lift around
  40 … 60 Hz, a notch near 125 Hz).
- **Low:** works on the deep lows (below ~80 Hz) and grows faster than the signal: its contribution at 50 Hz is
  −36 dB re the input at −30 dBFS and −12 dB at −6 dBFS (about 2 dB per dB), with a second harmonic 19 dB under it;
  less again at 0 dBFS (−18 dB): a level-dependent, even-harmonic bass path.
- **Loud:** upward compression, the same at every frequency: +6.6 dB at −60 dBFS, +5.1 at −48, +3.4 at −30, +1.9
  at −12, +0.5 at 0, +0.3 at +6 (1 kHz). Fast: down within 4 ms when the level jumps, back up over ~20 ms. The fast
  gain gives odd harmonics at low frequencies (h3 −34 dB at 100 Hz).
- **Grit:** no change to steady tones. On a kick-like hit it adds a burst 3 … 18 dB under the hit for its first
  ~20 ms, decaying to −50 dB by 40 ms. Its amount changes the processing, not only the mix (Section 5), and its
  detector listens inside the chain (Section 1).

## 12. Smoothing and switching

Changes at a block boundary (64-sample blocks), a 1 kHz tone at −20 dBFS:

| Change | Time to 90 % |
|---|---|
| a module's power (compression, lift) | ~5 ms crossfade |
| compression amount 0 → 1 | ~10 ms ramp |
| EQ gain 0 → 6 | ~5 ms |
| input gain 0 → +6 dB | 19 ms, exponential (τ ≈ 8 ms) |
| a parallel's power | within 2 ms |

## 13. CPU

Offline, one instance, 60 s of drums at 48 kHz (wall time less the ~1.2 s of loading): the default preset takes
8.3 s, i.e. **~14 % of a core**; everything on ~20 %. By module: drive 4.9 s (the most), compression 1.6, lift 1.7,
harmonics 0.2 (the neural model is cheap), EQ 0. The target for Scorpius is well below this.

Scorpius, measured the same way: the default preset 6.0 s (Orion 9.2 … 9.8 s in the same runs), drive alone 3.8 s
(5.7), lift 2.3 s (2.3), compression 0.4 s (2.1), grit alone 3.8 s (4.0). Its learned parts run a block at a time in vector registers
(`ScorpiusTests bench` times them separately).

## 14. What the fits found (Phase 4)

Fitting our DSP to the reference (nulls on drums, a mastered track and tones; NULLTESTS.md for the numbers) pinned
down more of how it is built:
- **Harmonic EQ** (`fit_eqfinal.py`, `fit_eqsat.py`, `fit_eqnull.py`, `fit_eqlaw.py`): the bands are RBJ-cookbook
  filters, all fed from the same input and summed in parallel (two bands on equal the sum of their contributions to
  −142 dB): a low shelf at 43.8 Hz (Q 0.70), bells with Q ≈ 2/3 at 100 Hz, 983 Hz and 3.01 kHz, a high shelf at
  9.66 kHz (Q 0.71). Each band's boost, (H − 1) x, is soft-clipped, u / (1 + |u|^2.15)^(1/2.15) with ceiling 0.47,
  and then high-passed at 20 Hz (Butterworth): the high-pass after the clip, not before, is worth 10 … 16 dB of null
  on the low bands. A knob's dB sets the filter's gain a little higher (+0.62 dB at 0.5, +3.65 at 3, +7.12 at 6).
- **Sub** (`fit_sub3.py`): the path is the chain's output doubled, through a 6th-order Butterworth low-pass at 140 Hz
  and high-pass at 15 Hz.
- **Harmonics** (`fit_harmlin.py`, `fit_harm.py`): the same pattern as an EQ band: a wide bell (Q 0.32, +5.8 dB) at
  1.4 times the chosen frequency (112 / 70 Hz), its boost soft-clipped and then high-passed at 20 Hz. That structure
  nulls at −43 … −48 dB; Scorpius now learns the fully wet process instead (`capture_nn.py harm80 / harm50`, amount
  1; a WaveNet as the drive's: −62 / −63 dB on held-out pieces, against −50 for an LSTM of the reference's own size,
  24 units) and mixes it by the measured amount weights; since the amount also changes the processing, at amounts
  0.25, 0.5 and 0.75 as well (each the fully wet equivalent x + (y − x) / w, `train_wn.py EQUIV`; −51 … −75 dB held
  out), the two models either side of the amount interpolated: at 0.8 −55 … −71 dB in place (one model: −46 … −52).
  The amount as an extra input of one model (`COND`) does worse at amount 1 (−37 … −44). The amount is not quite a mix: the output at amount a is
  (1 − w) x + w wet (+0.4 % of x) with w = 0.105 / 0.21 / 0.37 / 0.52 / 0.72 / 0.86 at a = 0.9 … 0.15, leaving
  −31 … −53 dB re the contribution that no filters on the two terms explain (the amount changes the processing).
- **Compression** (`fit_comp.py`): a feed-forward compressor on the mid signal: threshold −12.8 dBFS (detector level),
  ratio 5.0, knee 7.3 dB, detector attack 1.0 ms and release 20 ms (nearly a peak follower: 4 % RMS); the wet path's
  fixed shelving is a vector fit. Modes a and b fit the same constants; they differ in their wet filters (make-up).
  **The gain comes before the wet filters**, not after (`fit_comp_timing.py`): the two agree on steady tones but
  not when the gain moves fast through the filters' low cut (a kick's attack): loud drums −30.5 → −47.5 dB (mode
  b), −29.7 → −40.2 (a). Neither a hold before the release, a detector low shelf, feedback from the output nor a
  detector high-pass helps further. **The colour learned** (`prep_comp.py`, `capture_nn.py comp_a / comp_b`): our
  gain in front (the same arithmetic as Compressor.h), the reference's wet output as the target, a WaveNet beside a
  long filter (`train_wn.py BYPASS=1 KERNEL=3 MIXIN=1`, 4 channels, 8 layers: 8 channels and 10 layers do no
  better). On held-out pieces the best linear filter after our gain nulls at −36.6 (a) / −42.2 dB (b), the learned
  colour at −43.8 / −54.0. In the plug-in it gains less: drums −39.8 → −40.0 (a), −47.8 → −48.5 (b), the track
  −46.2 → −47.2 / −48.3 → −49.4, as loud material is limited by the gain's dynamics, which the network does not
  correct even when it hears the dry signal too (`XCOND`: no better). For about twice the plug-in's CPU, so it is
  off by default (`compLearned`). Mode a's one bad held-out piece (a quiet excerpt, −28 dB with either model) is a
  start-up effect: −19 … −22 dB over the first 1.2 s, then −38 … −45 like the rest; only quiet input after the
  plug-in is created shows it (perhaps a recurrent model settling from its initial state: unverified).
- **Low** (`fit_low2.py`): a subharmonic synthesizer, an octave below the bass: an **octave divider times an
  envelope**. On a steady tone its contribution is a square wave at half the frequency whose height follows the
  tone's level (at 25 Hz: −0.9 x the tone's amplitude for one cycle, +0.9 for the next), not the input times ±1 (the
  first model, `fit_low.py`, which nulled tones at only −1 … −10 dB). A flip-flop toggles at each falling zero
  crossing of the input through a 6th-order low-pass at 217 Hz (silent until the first, starting negative, keeping
  its state through silence: a reset makes bursts worse); its square wave times the peak envelope of that signal
  (attack 0.05 ms, release 50 ms) times 0.89 goes through a **4th-order Butterworth low-pass at 46.4 Hz** (a tone
  sweep 25 … 160 Hz at −30 dBFS follows it to 0.5 dB), then v + 0.91 v² (the DC it makes is kept: no high-pass) and
  a soft limit. Tones and chords null at −35 … −46 dB re the contribution (60 + 90 Hz gives 45 and 15 Hz, 40 + 60
  + 100 Hz gives 50 and 10 Hz). On music the octave's shape matches (correlation ±0.99 … 1.00 in most 50 ms
  windows of the drums) but its polarity slips against the reference's now and then, so the null is about 0 dB:
  some crossings count differently (not a fixed or level-relative hysteresis, not a minimum interval between
  flips, not a reset in silence). Loud tones (−3 dBFS) null at only −10 dB (the soft limit). No factory preset uses
  it.
- **Grit** works on **each channel by itself**: the left channel's gain is the same whatever the right one holds
  (silent, half, inverted), except when both are exactly equal and near full scale (the same drums at +0.4 dBFS
  peak: then it differs by −10 dB; at 0.7 of that, or with the right channel at 0.999 of the left, it does not).
  It adds nothing to steady tones; at an onset it adds a burst within the first 10 ms that decays by ~20 dB per
  20 … 25 ms, band-limited (at −24 dBFS: −5 … −8 dB re the tone at 300 Hz … 1 kHz, −14 at 3 kHz, −25 at 100 Hz),
  mostly at the tone's own frequency and in phase with it, with odd harmonics once it saturates (−12 dBFS: up to
  +2 dB over the tone at 1 kHz). Its contribution has nothing below 100 Hz, no linear filter of the input explains
  it (−1 dB), nor a gain on a filtered copy updated every millisecond (−4 … −10 dB). The level fader scales the
  contribution exactly (above). A transient shaper on the full band (`fit_grit.py`, the first attempt) did not
  correlate with it at all; Scorpius now learns it (`capture_nn.py grit`, left channel only, the right silent; a
  WaveNet as the drive's), nulling the parallel at −24 … −27 dB at level 1 (leaving it out: −20 … −25) and the
  preset "top of the morning" at −30 … −33.5 (−25 … −28.5 before).
- **Grit after compression:** on stereo material grit inside the chain is not the reference's grit applied to the
  compressor's output (−6 … −11 dB re its contribution; exact for identical channels), nor to its input, nor to
  its mid or swapped channels. Matters little: the factory presets with both use grit at 0.189.
- **Loud** (`fit_loud.py`): its gain against a fast mid-signal detector as a table; the detector is a peak follower
  with an instant attack and a 9.9 ms release (`fit_loud_timing.py`, on level steps and music: the parallel's
  error re its contribution −9 … −16 → −19 dB).
- **The expander** (`fit_exp.py`): threshold −58.1 dBFS (peak of the last 1.13 s), 2.03 dB of gain per dB below,
  closing with a 208 ms time constant; at the start of playback it takes the signal's own level.
- **Rates at 96 and 192 kHz** (`fit_converters.py`): the reference's converters there are a decimating FIR (the
  stream's first sample kept, then every 2nd / 4th) and an interpolating FIR, **not linear-phase** (no windowed sinc
  of any length its delay allows reaches its response near 20 … 24 kHz). Its responses to impulses at every phase fix
  the pair (y_c[t] = Σ over q ≡ −c (mod R) of d[q] u[t + S − q], fitted by alternating least squares to −76 / −78 dB);
  the split between the two is free up to a filter in z^R, taken with the decimator at unity gain. With them the
  round trip nulls at −86 … −92 dB on music and the presets as at 48 kHz (the split with the decimator 2.5 dB hot
  drove the drive harder: −6 … −15 dB).
- **Rates at 88.2 and 176.4 kHz**: rational polyphase converters. The responses to impulses repeat exactly every 147
  host samples (80 / 40 inner samples), so the inner instants fall 147 / 80 (147 / 40) host samples apart and one pair
  of filters on the grid of 1 / 80 (1 / 40) host sample describes them: v[k] = Σ_j x[j] d[147 k − 80 j], y[m] =
  Σ_k v[k] u[80 (m + S) − 147 k]. The 96 kHz pair in continuous time fits only −34 dB (another design), but it starts
  the fit: alternating least squares in which both steps split into small independent problems (each d tap belongs to
  one impulse phase; the outputs at 80 (m + S) ≡ c (mod 147) touch only the u taps ≡ c), with a line search, both
  filters causal. 88.2 kHz: fit −90 dB, music round trip −110 dB; 176.4 kHz converges slowly (−70 / −73 dB).
- **Rates** (`Resampler.h`): the reference's round trip delays 27.0 / 31.0 / 48.742 / 51.1 / 81.461 / 87.085 samples
  at 32 / 44.1 / 88.2 / 96 / 176.4 / 192 kHz (phase slope of white noise), reporting 27 / 31 / 46 / 48 / 75 / 80; at
  44.1 kHz it is flat to 22 kHz (−0.3 dB).
- **Drive and lift** are learned models in the reference too, as harmonics is said to be: neither a filter and a
  shaper in any order (Wiener, Hammerstein, both, with learned FIR filters: −14 dB at best on held-out music) nor a
  short-memory network reproduces them; the drive's peaks wander irregularly far above full scale. Scorpius learns
  its own models of them from 174 s of program material per setting (`capture_nn.py`): the drive a small WaveNet
  (8 channels, 12 for colour a with the hi-pass; 12 layers of dilations 1 … 2048 with 3 taps each, `train_wn.py`),
  one per colour and hi-pass setting (the hi-pass
  changes the drive's sound on music by more than its low cut suggests), lift's wet path a WaveNet too (−48.1 dB on held-out pieces after 60k steps, a GRU of 16 units
  −39.8), each between a learned short FIR and a learned 8192-tap FIR. Trained on the GPU when there is one. On
  held-out pieces up to +4 dBFS peak the drive's models null at −28 (colour a) to −36 dB (c). On the loud mastered
  track the drive's error is broadband (−11 … −20 dB in every band), the same in loud and quiet passages, as large as
  the signal in its quietest samples: dense intermodulation the model gets slightly wrong. The reference's drive is
  time-invariant (a time-shifted input nulls at −67 dB) and does not depend on the block size, so better is
  possible. What does not help colour a: more channels or layers (16 channels −25.5 against −24.6), a longer
  memory (dilations to 8192 or 32768, 0.34 / 1.4 s: −24.1 / −24.0), slow envelopes of the input as extra inputs
  (10 ms … 1 s: −23.8), a long pre-filter (−24.2), LSTMs (1 x 24: −19.7; 3 x 16 / 3 x 24: −20.5 / −21.0 after 20k
  steps; 3 x 24 after 100k: -23.4, its loudest piece -13.7). The reference's binary names its neural models
  (RESEARCH.md: "NLS Buss Drive A … LSTM_3_LAYER", other "NLS Buss" LSTMs of 2 layers, "Black Box", "Decapitator",
  "Shadow Hills" of 1): drive colour a is very likely a 3-layer LSTM; ours do not train as well as the WaveNet.
  The reference's drive is not chaotic (noise added at -100 … -60 dBFS moves its output in proportion) and depends
  on no future input; a linear filter explains only 3 … 4 dB of its output even 10 dB below full scale. The loud
  pieces are the limit even in training: the model fits the quiet ones to -29 dB and the loud ones to -16 … -19,
  trained or held out alike; trained on one loud piece alone it reaches -21 (8 channels) / -26 dB (16). What helps:
  **the network's output beside the long filter, not through it** (y = Post(u) + WaveNet(u)): colour a -24.6 ->
  -26.1 dB with 8 channels, -27.2 with 12 (its loudest piece -16.3 -> -18.7 / -20.0), 60k steps; the default preset
  on the track -16.1 -> -20.8 dB. For colours b and c it is better on loud pieces, worse on quiet ones. Then
  **dilated convolutions of 3 taps, and the input mixed into every layer** (train_wn.py KERNEL=3 MIXIN=1, still
  beside the filter): 3 … 5 dB better on held-out pieces for every colour (loudest piece 1.4 … 2 dB), colour a
  hi-pass -29.7 (-21.8) with 12 channels; in place the default preset 1.5 … 1.8 dB better (drums -26.6, track
  -22.4), c mode 1.5 … 3.6, low end theory 1 … 3. A longer run of the plain structure (120k steps, 12 channels) and
  a loss weighted against loud passages (LOSSP) gain less; an LSTM like the reference's (3 layers of 24) -23.4
  (-13.7). The drive costs about what the reference's does (5.6 against 5.8 s of a core per minute).

## Open

- The drive on loud, dense material (all of the default preset's error), compression mode a on drums (−40 against
  b's −48), which crossings the low path's flip-flop counts on music, its soft limit on loud bass, grit after the
  compressor on stereo material, mode a's start-up on quiet input, a cheaper learned compression colour.
- Tried and dropped (2026-10-28): an LSTM drive (3 layers of 24, as the binary's model names suggest) trained from
  the state half a second of material leaves instead of a zero state 512 samples before each window
  (`train_gru.py STATEWARM=24000`): −20.9 dB held out against −21.0 without it, both well behind the WaveNet's
  −29.7, so the short warm-up was not why the LSTMs lost.
