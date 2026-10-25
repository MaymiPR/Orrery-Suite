# Changelog

## Unreleased

In **The Antiparticle**: fine adjustment of the knobs is Shift only, as on The God Particle (Ctrl now drags at the
normal speed).

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
