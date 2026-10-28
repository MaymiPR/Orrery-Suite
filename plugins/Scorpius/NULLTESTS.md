# Null tests

The same signal and settings through the reference (Cradle Orion, VST3) and through Scorpius. The residual is
reference minus ours, in dB re the reference's output energy. Lower is better: -20 dB means the difference is a tenth
of the signal's amplitude, -60 dB is inaudible in practice. Both renders are compensated by each plug-in's reported
latency, block 512; the first 0.1 s is skipped (the reference ramps its settings in over the first ~5 ms). Settings
are applied 400 ms before each render (the reference applies them asynchronously; MEASUREMENTS.md).

Signals: **drums**, a synthetic drum loop (`nulltest/order.py`), peaks near full scale, and the same 12 dB lower;
**track**, 6 s of a mastered track (local only, not in the repository), and the same 10 dB lower.

Reproduce: `python nulltest/nullchain.py model` (our processor through tools/ScRender) or `python nulltest/nullchain.py
ours` (the built VST3; the same numbers), `python nulltest/nullmod.py` for the EQ bands. The reference renders stay
local (git-ignored).

## The whole plug-in: the factory presets

48 kHz (44.1 kHz within 0.2 dB of these, except "all off"):

| Preset | drums | drums -12 | track | track -10 |
|---|---|---|---|---|
| start me up (default) | -26.6 | -34.6 | -22.4 | -31.2 |
| c mode (drive colour c, compression mode a) | -32.6 | -40.2 | -24.8 | -36.8 |
| free at last | -26.5 | -34.6 | -22.3 | -31.0 |
| louder | -27.1 | -33.8 | -22.6 | -31.5 |
| low end theory | -26.5 | -33.9 | -22.4 | -29.8 |
| parallel me in | -26.8 | -34.5 | -22.3 | -31.2 |
| top of the morning | -32.9 | -36.6 | -33.4 | -35.1 |
| all off, 48 kHz | exact | exact | exact | exact |
| start me up with the drive off (the rest of the chain) | -48.2 | -51.0 | -44.6 | -49.2 |
| all off, 44.1 kHz | -51.5 | -51.5 | -63.7 | -63.7 |
| all off, 88.2 kHz | -109.2 | -109.2 | -109.7 | -109.7 |
| all off, 96 kHz | -89.7 | -89.7 | -92.2 | -92.2 |
| all off, 176.4 kHz | -73.1 | -73.1 | -73.3 | -73.3 |
| all off, 192 kHz | -86.2 | -86.2 | -86.4 | -86.4 |
| start me up, 88.2 kHz | -24.8 | -34.1 | -20.7 | -30.6 |
| start me up, 96 kHz | -25.6 | -34.1 | -21.1 | -30.7 |
| start me up, 176.4 kHz | -24.6 | -32.1 | -21.7 | -30.5 |
| start me up, 192 kHz | -26.3 | -32.3 | -22.0 | -30.8 |

## Module by module

Each module alone (the others off with the reference's own power buttons), on the same four signals:

| Module | Residual (dB) |
|---|---|
| EQ, one band at +6 dB | -55 … -67 |
| EQ, all bands at +4 dB | -46 … -48 |
| sub | -65 … -72 |
| harmonics (learned), amount 1 / 0.8 / 0.6 / 0.3 | -50 … -65 / -55 … -71 / -59 … -67 / -66 … -67 |
| compression, mode b / a (the gain corrected by a learned model) | -52.3 … -59.5 / -41.4 … -53.1 |
| loud, level 1 | -31 … -39 |
| lift (learned) | -43.5 … -51.5 |
| drive colour a, hi-pass on (learned, the default) | -22 … -34 |
| grit (learned), level 1 / 0.731 | -27 … -31 / -33 … -37 |
| low, tones and chords | -38 … -46 |
| low, music | about 0 (the octave's polarity slips against the reference's) |

The learned drive models on held-out pieces of their training material (up to +4 dBFS peak), mean (loudest piece):
colour a -29.7 (-21.8) / -28.3 (-20.5) dB (hi-pass on / off), colour b -32.1 (-23.6) / -33.6 (-23.7), colour c -34.6
(-25.5) / -36.3 (-25.6).

## What the numbers say

- **The fixed parts are close to exact** (EQ, sub, the rate converters: -50 to -70 dB): these are the reference's own
  filters, identified from its responses.
- **Everything but the drive** nulls at -44 … -51 dB together (the default preset with the drive off): the
  compressor applies its gain before its filters (as the reference), lift and harmonics are learned (harmonics at
  four amounts, interpolated).
- **The whole plug-in is limited by the drive**, which the reference runs as a learned model of its own and which
  Scorpius learns from the reference's input and output: on loud program material the default preset nulls at about
  -22 to -27 dB, 10 … 12 dB lower at -31 to -35 dB. Loud, dense material is the hard part: a linear filter explains
  only 3 dB of the drive's output there, and even a model trained on one loud piece alone reaches only -21 (8
  channels) to -26 dB (16) on it.
- **Above 48 kHz** (88.2, 96, 176.4, 192 kHz) Scorpius runs the reference's own converters, identified from its
  impulse responses (-73 … -110 dB with everything off); the presets null as at 48 kHz.
- **Grit** is learned too (a transient shaper of the mids, each channel by itself), 5 … 9 dB better than a
  hand-made transient shaper; after the compressor on stereo material the reference's grit still differs a little.
- **Low** matches on tones and chords; on music its octave keeps the right shape and level but its polarity drifts
  against the reference's (which crossings its flip-flop counts is not pinned down).

## CPU

60 s of the drum loop, stereo, 48 kHz, one instance, offline, the two plug-ins in the same run: the default preset
takes 10.6 … 10.8 s of CPU against the reference's 9.1 s (drive alone 4.6 s against 5.7, lift 3.3 against 2.3,
compression 0.5 against 2.1, grit 2.8 against 4.1). The learned parts run in 8-wide vector registers where the
processor has AVX.
