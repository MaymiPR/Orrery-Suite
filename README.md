<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/branding/orrery-banner-dark.svg">
    <img src="docs/branding/orrery-banner-light.svg" alt="Orrery Suite: Cradle's mix-bus processors, recreated and null-tested" width="100%">
  </picture>
</p>

<p align="center">
  <a href="https://github.com/MaymiPR/OrrerySuite/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/MaymiPR/OrrerySuite?color=b07cff&label=release"></a>
  <img alt="Formats" src="https://img.shields.io/badge/formats-VST3%20%7C%20CLAP%20%7C%20AU-25202f">
  <img alt="Platforms" src="https://img.shields.io/badge/platforms-Windows%20%7C%20macOS%20%7C%20Linux-25202f">
  <a href="LICENSE"><img alt="License: AGPL v3" src="https://img.shields.io/badge/license-AGPL--3.0-9b6cff"></a>
</p>

# Orrery Suite

Open-source mix-bus processors (**VST3**, **CLAP**, **AU**) for Windows, macOS and Linux, built with JUCE. Each Orrery
plug-in is an independent, from-scratch recreation of a well-known mixing tool: its controls and look studied, its sound
measured black-box, and the result null-tested against the original, with the numbers published as they are.

An orrery is a clockwork model of the heavens that reproduces their motions. Each Orrery plug-in is a working model
of a well-known original in the same spirit.

> Orrery Suite is an independent project. It is not affiliated with, endorsed by or sponsored by Cradle or Jaycen
> Joshua. Other companies' product names appear only to describe what a Orrery plug-in is modelled on and belong to
> their owners. No code of theirs is included: the sound and the look were measured from the outside.

## Download

One installer per operating system installs every plug-in in the suite. Get it from
[Releases](https://github.com/MaymiPR/OrrerySuite/releases):

| System | File | Installs to |
|---|---|---|
| Windows 10 / 11 (64-bit) | `OrrerySuite-<version>-Windows-Setup.exe` | `Common Files\VST3\Orrery Suite`, `Common Files\CLAP\Orrery Suite`; standalone apps optionally |
| macOS 10.13+ (Apple Silicon and Intel) | `OrrerySuite-<version>-macOS.pkg` | `/Library/Audio/Plug-Ins/{Components, VST3/Orrery Suite, CLAP/Orrery Suite}` |
| Linux x86-64 (glibc 2.35+) | `OrrerySuite-<version>-Linux.tar.gz` | `~/.vst3/Orrery Suite`, `~/.clap/Orrery Suite` (run `install-linux.sh`) |

The installers are not code-signed or notarized; see [packaging/](packaging) for the first-run steps on each system.
Each release lists the files' SHA-256 checksums.

## Plug-ins

| Orrery plug-in | Modelled on | Status |
|---|---|---|
| **[The Antiparticle](plugins/TheAntiparticle)** | Cradle The God Particle: mix-bus EQ, multiband character and limiter | released: close, not a null on every material (see below) |
| **Orion** (working title) | Cradle Orion | next |

## The Antiparticle

<p align="center">
  <img src="docs/screenshots/the-antiparticle@2x.png" alt="The Antiparticle" width="80%">
</p>

The God Particle's chain, rebuilt: input gain, a three-band EQ, the "character" section (a 3-band linear-phase split,
each band a stereo-linked parallel compressor and a sine shaper, amount 0 – 200 %), a look-ahead limiter with a
−0.1 dBFS ceiling, output gain. Oversampling 1 – 8x, an external side-chain for the character section's detectors, the
original's editor, meters and particle visualiser. Two additions of ours: an **I/O LINK** switch (the output follows
the input gain inverted, so driving the chain harder doesn't change the level) and an "antiparticle" particle style.
More: [plugins/TheAntiparticle](plugins/TheAntiparticle).

### Null test (honest numbers)

Residual of The Antiparticle against the original, in dB re the original's output (lower is better; −60 dB is
inaudible in practice):

- **A mastered track** (future house, about −10 dBFS RMS) at the default settings: **−44.3 dB**; the limiter alone
  −60.8 dB, the character section alone −46.1 dB. Character at 125 / 150 / 175 / 200 %: −44.0 / −42.5 / −40.9 / −39.4 dB.
- **Synthetic music** −62 to −74 dB at the defaults; the path, EQ and crossover −79 to −87 dB; amount 0 % −135 dB.
- **Stress suite:** 163 cases over the settings, levels, signals, automation, sample rates and block sizes: **median
  −63.3 dB**, worst −40.4 dB.
- **Weakest:** loud, dense, bright material through the character section's high band (pink noise at −6 dBFS RMS
  −40 dB). Its steady gain curve matches to 0.02 dB; what is left is how fast its detector moves.
- **How:** the crossover filters were identified exactly from the original's impulse responses; the limiter's
  look-ahead planning was identified with probe signals (its inter-sample interpolator fitted per sample rate); the
  character section's laws were fitted against the original on program material and probes. The meters, knobs and
  particles were filmed from the original's editor and matched frame by frame.

Every case: [NULLTESTS.md](plugins/TheAntiparticle/NULLTESTS.md). How it was measured:
[MEASUREMENTS.md](plugins/TheAntiparticle/MEASUREMENTS.md). The editor and controls:
[RESEARCH.md](plugins/TheAntiparticle/RESEARCH.md).

## Building

CMake 3.22+ and a C++17 compiler. JUCE 8 and clap-juce-extensions are fetched by CMake.

```bash
cmake -B build -DAP_TOOLS=OFF
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

That builds every plug-in in every format for the platform (VST3, CLAP and standalone; AU on macOS) and the unit tests.
Without `-DAP_TOOLS=OFF` the measurement tools are built too: hosts for the reference plug-ins (`tools/`) and each
plug-in's fitting helpers. The Windows installer is made with Inno Setup 6:

```bash
pwsh packaging/make_iss_include.ps1 build
iscc /DAppVersion=1.0.0 packaging/OrrerySuite.iss
```

## Repository layout

| Path | What it holds |
|---|---|
| `plugins/<Name>/` | one plug-in: `Source/`, `resources/`, `tests/`, its null-test harness (`nulltest/`, Python, needs the original installed), its fitting tools and its documents |
| `tools/` | hosts for reference plug-ins shared by the suite: offline renders (RefRender), editor films and screenshots (RefCapture), parameter lists (ParamDump) |
| `packaging/` | the installers; `plugins.txt` lists the plug-ins they hold |
| `docs/` | branding and screenshots |
| `.github/workflows/release.yml` | builds, tests and validates every plug-in on all three systems (pluginval at strictness 10, auval on macOS), makes the installers and, on a `v*` tag, publishes a release with the notes from [CHANGELOG.md](CHANGELOG.md) |

Reference renders, measurement data and anything taken from the original plug-ins stay out of the repository (see
`.gitignore`); only the write-ups are published.

## Versions

1.0.0 is the first release, with The Antiparticle. Each new plug-in raises the middle number (Orion will be 1.1.0, the
next one 1.2.0), and fixes and small updates in between raise the last (1.1.1, 1.1.2 ...). Every plug-in carries the
suite's version; [CHANGELOG.md](CHANGELOG.md) lists what changed in each.

## License

[AGPL-3.0](LICENSE). The Antiparticle's interface font, Instrument Sans, is under the
[SIL Open Font License 1.1](plugins/TheAntiparticle/resources/fonts/OFL-InstrumentSans.txt).
