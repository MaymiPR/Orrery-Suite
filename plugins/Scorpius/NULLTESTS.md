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
| start me up (default) | -24.9 | -32.8 | -20.8 | -29.7 |
| c mode (drive colour c, compression mode a) | -31.1 | -36.6 | -23.3 | -34.6 |
| free at last | -24.9 | -32.7 | -20.7 | -29.5 |
| louder | -25.2 | -32.4 | -20.9 | -29.7 |
| low end theory | -24.3 | -30.8 | -21.0 | -28.8 |
| parallel me in | -25.0 | -32.8 | -20.7 | -29.8 |
| top of the morning | -32.9 | -36.6 | -33.4 | -35.1 |
| all off, 48 kHz | exact | exact | exact | exact |
| start me up with the drive off (the rest of the chain) | -46.8 | -50.7 | -44.1 | -48.0 |
| all off, 44.1 kHz | -51.5 | -51.5 | -63.7 | -63.7 |
| all off, 88.2 / 96 / 192 kHz (drums, track) | -20.6 / -21.1 / -22.6 | | -27.7 / -28.3 / -31.4 | |

## Module by module

Each module alone (the others off with the reference's own power buttons), on the same four signals:

| Module | Residual (dB) |
|---|---|
| EQ, one band at +6 dB | -55 … -67 |
| EQ, all bands at +4 dB | -46 … -48 |
| sub | -65 … -72 |
| harmonics (learned), amount 1 / 0.8 / 0.6 / 0.3 | -50 … -65 / -55 … -71 / -59 … -67 / -66 … -67 |
| compression, mode b / a | -47.5 … -58 / -40 … -50 |
| loud, level 1 | -31 … -39 |
| lift (learned) | -43.5 … -51.5 |
| drive colour a, hi-pass on (learned, the default) | -19 … -31 |
| grit (learned), level 1 / 0.731 | -27 … -31 / -33 … -37 |
| low, tones and chords | -38 … -46 |
| low, music | about 0 (the octave's polarity slips against the reference's) |

The learned drive models on held-out pieces of their training material (up to +4 dBFS peak), mean (loudest piece):
colour a -27.2 (-20.0) / -26.3 (-19.4) dB (hi-pass on / off), colour b -29.2 (-22.2) / -30.1 (-22.3), colour c -31.6
(-24.0) / -31.0 (-24.0).

## What the numbers say

- **The fixed parts are close to exact** (EQ, sub, the rate converters: -50 to -70 dB): these are the reference's own
  filters, identified from its responses.
- **Everything but the drive** nulls at -44 … -51 dB together (the default preset with the drive off): the
  compressor applies its gain before its filters (as the reference), lift and harmonics are learned (harmonics at
  four amounts, interpolated).
- **The whole plug-in is limited by the drive**, which the reference runs as a learned model of its own and which
  Scorpius learns from the reference's input and output: on loud program material the default preset nulls at about
  -21 to -25 dB, 10 … 12 dB lower at -30 to -33 dB. Loud, dense material is the hard part: a linear filter explains
  only 3 dB of the drive's output there, and even a model trained on one loud piece alone reaches only -21 (8
  channels) to -26 dB (16) on it.
- **Above 48 kHz** the rate converters are the limit (-21 … -31 dB with everything off): the reference's response
  near 20 … 24 kHz (and its phase there) is not a windowed sinc of any length its delay allows.
- **Grit** is learned too (a transient shaper of the mids, each channel by itself), 5 … 9 dB better than a
  hand-made transient shaper; after the compressor on stereo material the reference's grit still differs a little.
- **Low** matches on tones and chords; on music its octave keeps the right shape and level but its polarity drifts
  against the reference's (which crossings its flip-flop counts is not pinned down).

## CPU

60 s of the drum loop, stereo, 48 kHz, one instance, offline, the two plug-ins in the same run: the default preset
takes 10.6 … 10.8 s of CPU against the reference's 9.1 s (drive alone 4.6 s against 5.7, lift 3.3 against 2.3,
compression 0.5 against 2.1, grit 2.8 against 4.1). The learned parts run in 8-wide vector registers where the
processor has AVX.
