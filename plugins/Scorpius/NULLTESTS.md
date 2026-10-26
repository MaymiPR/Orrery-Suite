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

48 kHz (44.1 kHz within 0.4 dB of these, except "all off"):

| Preset | drums | drums -12 | track | track -10 |
|---|---|---|---|---|
| start me up (default) | -21.2 | -31.0 | -16.1 | -24.6 |
| c mode (drive colour c, compression mode a) | -29.4 | -39.4 | -21.4 | -34.1 |
| free at last | -21.1 | -30.9 | -16.2 | -24.6 |
| louder | -21.4 | -30.1 | -16.0 | -24.3 |
| low end theory | -22.4 | -32.0 | -16.6 | -25.5 |
| parallel me in | -21.2 | -31.1 | -15.9 | -24.5 |
| top of the morning | -32.9 | -36.6 | -33.4 | -35.1 |
| all off, 48 kHz | exact | exact | exact | exact |
| start me up with the drive off (the rest of the chain) | -43.8 | -47.4 | -41.4 | -44.4 |
| all off, 44.1 kHz | -51.5 | -51.5 | -63.7 | -63.7 |

## Module by module

Each module alone (the others off with the reference's own power buttons), on the same four signals:

| Module | Residual (dB) |
|---|---|
| EQ, one band at +6 dB | -55 … -67 |
| EQ, all bands at +4 dB | -46 … -48 |
| sub | -65 … -72 |
| harmonics (learned), amount 0.8 / 1 | -46 … -52 / -51 … -68 |
| compression, mode b / a | -47.5 … -58 / -40 … -50 |
| loud, level 1 | -31 … -39 |
| lift (learned) | -41 … -50 |
| drive colour c (learned) | -21 … -34 |
| drive colour a (learned) | -16 … -31 |
| grit (learned), level 1 / 0.731 | -27 … -31 / -33 … -37 |
| low, tones and chords | -38 … -46 |
| low, music | about 0 (the octave's polarity slips against the reference's) |

The learned drive models on held-out pieces of their training material (up to +4 dBFS peak): colour a -24.6 / -24.8
dB (hi-pass on / off), colour b -31.7 / -31.4, colour c -33.4 / -32.9.

## What the numbers say

- **The fixed parts are close to exact** (EQ, sub, the rate converters: -50 to -70 dB): these are the reference's own
  filters, identified from its responses.
- **Everything but the drive** nulls at -41 … -47 dB together (the default preset with the drive off): the
  compressor applies its gain before its filters (as the reference), lift and harmonics are learned.
- **The whole plug-in is limited by the drive**, which the reference runs as a learned model of its own and which
  Scorpius learns from the reference's input and output: on loud program material the default preset nulls at about
  -16 to -21 dB, 12 dB lower at -25 to -31 dB. Colour c, which Scorpius models best, takes the chain to -21 … -39.
  What is left of the drive is mostly below 250 Hz; wider or deeper models gain only about 1 dB.
- **Grit** is learned too (a transient shaper of the mids, each channel by itself), 5 … 9 dB better than a
  hand-made transient shaper; after the compressor on stereo material the reference's grit still differs a little.
- **Low** matches on tones and chords; on music its octave keeps the right shape and level but its polarity drifts
  against the reference's (which crossings its flip-flop counts is not pinned down).

## CPU

60 s of the drum loop, stereo, 48 kHz, one instance, offline: the default preset takes 6.0 s of CPU against the
reference's 9.2 s in the same run (drive alone 3.8 s against 5.7, grit alone 3.8 s against 4.0).
