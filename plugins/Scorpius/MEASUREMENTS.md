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
| grit level | 0.088 / 0.241 / 0.505 | −36 … −51: the level also changes the processing |

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

## Open for Phase 4

- Fit each module's model (filters around its saturator; the compressor's and the parallels' dynamics) against
  program material, as for The Antiparticle; the two compression modes on drums.
- The low-level expander's exact hold and release curve.
- The GR readout's quantity against the compressor's real reduction (UI round 4 measured the readout).
- Whether to run at a fixed 48 kHz with the reference's resampler (needed for nulls at 44.1 kHz) or at the host rate.
