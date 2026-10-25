# The Antiparticle

An open-source mix-bus processor (VST3 / AU / CLAP) for Windows, macOS and Linux that recreates the controls, look and
sound of Cradle's The God Particle, null-tested against the original.

- [RESEARCH.md](RESEARCH.md): what the original does, and the UI measurements.
- [MEASUREMENTS.md](MEASUREMENTS.md): how its audio behaves, measured black-box.
- [NULLTESTS.md](NULLTESTS.md): how closely The Antiparticle matches it, in dB of residual.
- [CHANGELOG.md](CHANGELOG.md): what changed in each release.

Independent project, not affiliated with or endorsed by Cradle or Jaycen Joshua. Product names belong to their owners
and are used only to describe what this project is compatible with.

## Installing

Download the installer for your system from the [Releases](https://github.com/MaymiPR/TheAntiparticle/releases) page.

| System | File | Installs |
|---|---|---|
| Windows 10 / 11 (64-bit) | `TheAntiparticle-<version>-Windows-Setup.exe` | VST3 and CLAP to `C:\Program Files\Common Files\VST3` / `CLAP`; the standalone app optionally |
| macOS 10.13+ (Apple Silicon and Intel) | `TheAntiparticle-<version>-macOS.pkg` | AU, VST3 and CLAP to `/Library/Audio/Plug-Ins` |
| Linux (x86-64, glibc 2.35+) | `TheAntiparticle-<version>-Linux.tar.gz` | VST3 and CLAP to `~/.vst3` / `~/.clap` (run `sh install-linux.sh`) |

The installers are not code-signed: on Windows, SmartScreen may warn ("More info", then "Run anyway"); on macOS, open
the .pkg once, then allow it in System Settings > Privacy & Security ("Open Anyway"). Each release lists the files'
SHA-256 checksums. Details: [packaging/INSTALL-Windows.txt](packaging/INSTALL-Windows.txt),
[INSTALL-macOS.txt](packaging/INSTALL-macOS.txt), [INSTALL-Linux.txt](packaging/INSTALL-Linux.txt).

## What it does

Input gain feeds the signal chain below, which ends in output gain.

| Stage | What it is |
|---|---|
| EQ | Three RBJ filters: low shelf 180 Hz, bell 637 Hz, high shelf 10 kHz, ±6 dB each. |
| Character, "amount" 0–200 % | A 3-band split by a linear-phase FIR crossover (157 Hz / 2.68 kHz). Each band runs a stereo-linked parallel compressor (it listens to the mid signal) and then a sine shaper (odd harmonics); the side is widened per band. |
| Limiter | Look-ahead with a −0.1 dBFS sample-peak ceiling and a two-speed release; input gain 0 – 12 dB. |

Each stage has its own power button. Oversampling (1 / 2 / 4 / 8x) and the I/O meter mode (RMS or LUFS readouts) are
global settings (gear icon); as in the original, 8x sounds like 4x. INT / EXT picks the character section's detector
input: the main signal or the side-chain bus.

How close it is: on a mastered track at the default settings the difference to the original is about −44 dB (the
limiter alone −61 dB); on synthetic music −62 to −74 dB. The weakest case is loud, dense, bright material through the
character section's high band. [NULLTESTS.md](NULLTESTS.md) has every case.

## Using it

- **Knobs:** drag up / down. Hold Shift or Ctrl (Cmd on macOS) for fine adjustment. While the mouse is over a knob, its
  label shows the value.
- **I/O LINK** (top bar, ours): the output gain follows the input gain inverted, so you can drive the chain harder
  without the level changing.
- **Presets:** the arrows step through them; the save button stores the EQ, character and limiter settings as an
  `.antp` file in `Documents/The Antiparticle/Presets`. Input, output and INT / EXT are left as they are.
- **Settings** (gear): oversampling, meter mode, particle animation on / off and its style: "God particle" (the
  original's sparks) or "Antiparticle" (ours: bubble-chamber pair creation and annihilation). The particles follow the
  input level and fade with quiet passages.
- **Zoom:** 50 – 200 % (the box next to the gear). The logo opens the version and About.

## Building

CMake 3.22+ and a C++17 compiler. JUCE 8 and clap-juce-extensions are fetched by CMake.

```bash
cmake -B build -DAP_TOOLS=OFF
cmake --build build --config Release --target TheAntiparticle_VST3 TheAntiparticle_CLAP AntiparticleTests
ctest --test-dir build -C Release --output-on-failure
```

On macOS add `TheAntiparticle_AU`, on Windows `TheAntiparticle_Standalone` for the app. Without `-DAP_TOOLS=OFF` the
measurement tools are built too (hosts for the reference plug-in). The Windows installer is made with Inno Setup 6:
`iscc /DAppVersion=0.1.0 packaging\TheAntiparticle.iss` after a Release build (writes `dist\`).

## Tests and releases

- `AntiparticleTests`: unit tests of the DSP and the processor, including filters, the FIR crossover, latency, bypass,
  limiter ceiling, state and extreme input.
- `nulltest/`: the null tests against an installed copy of the original (Windows; not in CI). `nulltest.py` and
  `stress.py` use synthetic signals; `track.py` takes a music file of your own.
- `.github/workflows/release.yml` builds every format on all three platforms, runs the unit tests, validates the
  plug-ins with pluginval at strictness 10 (and auval on macOS) and makes the installers. It runs when a version tag is
  pushed (`git tag v0.1.0 && git push origin v0.1.0`), which publishes a GitHub Release with the notes from
  CHANGELOG.md, or by hand from the Actions tab.

## License

[AGPL-3.0](LICENSE). The interface font, Instrument Sans, is under the
[SIL Open Font License 1.1](resources/fonts/OFL-InstrumentSans.txt).
