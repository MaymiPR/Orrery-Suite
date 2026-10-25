# Changelog

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
  the input level. Zoom 50 – 200 %.
- **Additions of ours:** an I/O LINK switch (the output follows the input gain inverted, so level changes cancel) and an
  "antiparticle" particle style on the settings page.
- **Presets:** the original's default as a factory preset, and user presets saved as .antp files in
  Documents/The Antiparticle/Presets.
