# Changelog

## Unreleased

In **Scorpius**:

- **Switching a module on now sounds like the original's:** the drive, harmonics, compression and lift start from
  silence, as Orion's do (ours started from a state the models never saw in training, and the lift carried on from
  where it stopped). Around the switch, against the original: drive −8 → −28 dB, the others −24 … −30 → −40 … −42 dB,
  at every host block size.
- **The low-level expander** in front of those modules opens over about 5 ms, as the original's does, instead of at
  once, and never closes beyond −44.5 dB (it used to keep falling on very quiet input).

## 1.1.0

**Scorpius**, the suite's second plug-in: a recreation of Cradle's Orion (v1.2.1), the drum-bus processor, null-tested
against the original. VST3, CLAP and a standalone app on Windows; VST3 and CLAP on Linux; AU, VST3 and CLAP on macOS.

- **Signal chain:** input gain, drive (colours a / b / c, optional hi-pass), harmonics (50 or 80 Hz), compression
  (modes a / b, with a gain-reduction readout), lift and a five-band harmonic EQ, then the sub, low, loud and grit
  parallels mixed in, and output gain. The original's nine factory presets; your own save as `.scorpius` files.
- **How close:** the EQ, sub and rate converters are the original's own filters (−50 to −70 dB); everything but the
  drive nulls at −45 to −51 dB together; the default preset at about −22 to −27 dB on loud material, limited by the
  drive. Parameter changes and power switches follow the original's timing. See plugins/Scorpius/NULLTESTS.md.
- **Editor:** the original's layout, meters, knobs, faders and particle stripe, measured from it, with A / B compare.
  Ours on top: an external side-chain (INT / EXT) for the compressor, I/O LINK, and a "Scorpius" particle style
  (the default) next to the original's.

In **The Antiparticle**:

- **A / B compare** (ours, as Cradle's Orion has it): two complete settings in one instance, switched from the top bar;
  the arrow copies the active one over the other.
- **Oversampling at every sample rate:** its filters are now designed for the session's rate, as the original's are.
  At 44.1 kHz with 2x or 4x it used to differ clearly from the original (−14 dB on drums); now −61 dB. 96 kHz improves
  the same way (−19 → −57 dB), and 4x / 8x at 48 kHz a little (−53 → −59 dB).
- **Particles:** the "God particle" style now glows like neon around a black hole in the centre, the glass is clearer
  and reflects the sparks in its rims, and both styles move with the project's tempo.
- Fine adjustment of the knobs is Shift only, as on The God Particle (Ctrl now drags at the normal speed).

In both: double-click a knob to type a value. The suite's top bars match: A / B left of the preset menu, then the
side-chain switch and I/O LINK.

## 1.0.1

Fixes in **The Antiparticle**:

- **About no longer crashes the host.** Choosing "About The Antiparticle" from the logo's menu could crash the DAW;
  the menu now closes safely before the panel opens. The About panel is also larger, so its text has room.
- **The I/O readouts read like The God Particle's,** measured frame by frame from films of the original:
  - **RMS readout:** live instead of held. It updates about four times a second and shows the RMS + 1.5 dB, as the
    original does. It falls back to -inf when the audio stops, on the input and the output.
  - **Peak readout:** held until clicked, as before, but it now samples the level about every 46 ms as the original
    does. Very short spikes read lower than the sample peak, as they do on the original.
  - Below -96 dB a readout shows -inf.

## 1.0.0

The first release of Orrery Suite, with its first plug-in, **The Antiparticle**: a recreation of Cradle's The God
Particle (v1.2.4), its controls, look and sound, null-tested against the original. VST3, CLAP and a standalone app on
Windows; VST3 and CLAP on Linux; AU, VST3 and CLAP on macOS (universal).

- **Signal chain:** input gain, a three-band EQ, the character section (a 3-band FIR split with a parallel compressor and
  a sine shaper per band, amount 0 – 200 %), a look-ahead limiter at −0.1 dBFS, output gain. Oversampling 1 / 2 / 4 /
  8x and an external side-chain (INT / EXT) for the character section's detectors.
- **How close:** on a mastered future-house track at the default settings the difference to the original is about
  −44 dB, with the limiter alone −61 dB; on synthetic music −62 to −74 dB. The stress suite (163 cases over settings,
  levels, rates, block sizes and automation) has a median of −63 dB. See plugins/TheAntiparticle/NULLTESTS.md.
- **Editor:** the original's layout, knobs, glass visualiser and meters, measured from it: the value readout on a knob,
  Shift or Ctrl for fine adjustment, the I/O meters' colours and the gain-reduction LEDs, and particles that follow
  the input level. Zoom 50 – 200 %, from the menu or by dragging the bottom-right corner.
- **Additions of ours:** an I/O LINK switch (the output follows the input gain inverted, so level changes cancel) and an
  "antiparticle" particle style on the settings page.
- **Presets:** the original's default as a factory preset, and user presets saved as .antp files in
  Documents/The Antiparticle/Presets.
