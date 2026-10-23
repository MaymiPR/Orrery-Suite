# The Antiparticle

Open-source mix-bus processors (VST3 / AU / CLAP) for Windows, macOS and Linux, modelled on and null-tested against
familiar commercial plug-ins. The first one, **The Antiparticle**, recreates the controls, look and sound of Cradle's
The God Particle.

Work in progress. The documents so far:
- [RESEARCH.md](RESEARCH.md): what the original does, and the UI measurements.
- [MEASUREMENTS.md](MEASUREMENTS.md): how its audio behaves, measured black-box.
- [NULLTESTS.md](NULLTESTS.md): how closely The Antiparticle matches it, in dB of residual.

Independent project, not affiliated with or endorsed by Cradle or Jaycen Joshua. Product names belong to their owners
and are used only to describe what this project is compatible with.

## What it does

Input gain feeds the signal chain below, which ends in output gain.

| Stage | What it is |
|---|---|
| EQ | Three RBJ filters: low shelf 180 Hz, bell 637 Hz, high shelf 10 kHz, ±6 dB each. |
| Character, "amount" 0–200 % | A 3-band split by a linear-phase FIR crossover (157 Hz / 2.68 kHz). Each band runs a stereo-linked parallel compressor and then a cubic shaper (odd harmonics). |
| Limiter | Look-ahead with a −0.1 dBFS sample-peak ceiling and a two-speed release. |

Oversampling (1 / 2 / 4 / 8x) and the I/O meter mode are global settings, as in the original. The external side-chain
drives the character section's detectors.

## Building

CMake 3.22+ and a C++17 compiler. JUCE 8 and clap-juce-extensions are fetched by CMake.

```bash
cmake -B build
cmake --build build --config Release --target TheAntiparticle_VST3 TheAntiparticle_CLAP AntiparticleTests
ctest --test-dir build -C Release --output-on-failure
```

On macOS add `TheAntiparticle_AU`. `-DAP_TOOLS=OFF` skips the measurement tools (hosts for the reference plug-in).

## Tests

- `AntiparticleTests`: unit tests of the DSP and the processor, including filters, the FIR crossover, latency, bypass,
  limiter ceiling, state and extreme input.
- `nulltest/nulltest.py`: the null-test suite against an installed copy of the original (Windows; not in CI).
- CI (`.github/workflows/build.yml`) builds every format on all three platforms, runs the unit tests and validates the
  plug-ins with pluginval at strictness 10 (and auval on macOS).

## License
[AGPL-3.0](LICENSE).
