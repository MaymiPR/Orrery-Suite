# Working on Orrery Suite

Bug reports and null-test findings are welcome as
[issues](https://github.com/MaymiPR/Orrery-Suite/issues/new/choose). This page is for changing the code: how it's
built and tested, how a change is checked against the originals, and how a release is made.

## The one rule

Each plug-in follows its original, and the original is the reference for every behaviour. A change that affects the
sound is measured against the original's own render of the same settings (see [Null tests](#null-tests)), and what was
measured goes into the plug-in's MEASUREMENTS.md and NULLTESTS.md, so the next person can see why the code is the way
it is. Never commit the originals' renders, files or anything taken from them.

## Build

Requirements, and the commands per platform, are in the [build guide](docs/BUILDING.md). Building with the
measurement tools (the default, `-DAP_TOOLS=ON`) also makes the hosts for the reference plug-ins (`tools/`) and each
plug-in's offline renderer (ApRender, ScRender), editor snapshot and fitting helpers, in
`build/<tool>_artefacts/Release/`.

| Test | What it does |
|---|---|
| `AntiparticleTests` | The Antiparticle's DSP and processor: filters, the FIR crossover, latency, bypass, the limiter's ceiling, oversampling, state, extreme input. |
| `ScorpiusTests` | Scorpius's DSP and processor: the modules, presets, A / B, the side-chain, state; `bench` times the learned models. |

Both run with `ctest --test-dir build -C Release --output-on-failure` and return non-zero on a failure.

GitHub Actions builds both plug-ins and runs the tests on Windows, macOS and Linux (`.github/workflows/ci.yml`) for
every push to `main` and every pull request, and from Actions > *CI* > *Run workflow*. Changes to documents, `docs/`
or the null-test harnesses alone don't start it. The installers, CLAP, AU and the pluginval / auval checks are made
at each release (`release.yml`).

## Windows installer

Made with Inno Setup 6:

```bash
pwsh packaging/make_iss_include.ps1 build
iscc /DAppVersion=1.1.0 packaging/OrrerySuite.iss
```

The installer lands in `dist/`. Add `/DTestRoot=<folder>` to make a test installer that installs everything under that
folder without administrator rights.

## Repository layout

| Path | What it holds |
|---|---|
| `plugins/<Name>/` | one plug-in: `Source/`, `resources/`, `tests/`, its null-test harness (`nulltest/`, Python, needs the original installed), its tools and its documents (RESEARCH, MEASUREMENTS, NULLTESTS) |
| `shared/` | code and resources shared by the plug-ins: the UI library, the font |
| `tools/` | hosts for the reference plug-ins: offline renders (RefRender), editor films and screenshots (RefCapture), parameter lists (ParamDump) |
| `packaging/` | the installers; `plugins.txt` lists the plug-ins they hold |
| `docs/` | the build guide, branding and screenshots |
| `.github/workflows/` | `ci.yml` builds the plug-ins and runs the tests on all three systems for every push to `main` and pull request; `release.yml` builds every format, validates it (pluginval at strictness 10, auval on macOS), makes the installers and, on a `v*` tag, publishes a release with the notes from [CHANGELOG.md](CHANGELOG.md) |

Reference renders, measurement data and anything taken from the original plug-ins stay out of the repository (see
`.gitignore`); only the write-ups are published.

## Null tests

Each plug-in's `nulltest/` folder renders the same signals through the original and through ours and reports the
residual. It needs Windows, Python 3 with numpy and scipy, the original plug-in installed, and the tools built
(`-DAP_TOOLS=ON`). The entry points are listed in each plug-in's README; the results are in its NULLTESTS.md.

## Versions

One version for the whole suite (`project(... VERSION ...)` in `CMakeLists.txt`). Each new plug-in raises the middle
number (1.1.0 added Scorpius), fixes and small updates raise the last. A release is: raise the version, add a
`## x.y.z` section to [CHANGELOG.md](CHANGELOG.md), run the release workflow by hand as a test, then push the tag
`vx.y.z`.
