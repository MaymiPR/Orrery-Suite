# Working on Orrery Suite

Bug reports and null-test findings are welcome as
[issues](https://github.com/MaymiPR/OrrerySuite/issues/new/choose). This page is for changing the code: how it's
built and tested, how a change is checked against the originals, and how a release is made.

## The one rule

Each plug-in follows its original, and the original is the reference for every behaviour. A change that affects the
sound is measured against the original's own render of the same settings (see [Null tests](#null-tests)), and what was
measured goes into the plug-in's MEASUREMENTS.md and NULLTESTS.md, so the next person can see why the code is the way
it is. Never commit the originals' renders, files or anything taken from them.

## Build

CMake 3.22 or later and a C++17 compiler. JUCE 8 and clap-juce-extensions are fetched by CMake.

```bash
cmake -B build -DAP_TOOLS=OFF
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

This builds every plug-in in every format for the platform (VST3, CLAP and standalone; AU on macOS) and the unit
tests. The artefacts land in `build/<Target>_artefacts/Release/`.

- **Linux** needs JUCE's development packages first (Ubuntu / Debian):

  ```bash
  sudo apt-get install -y --no-install-recommends ninja-build libasound2-dev libjack-jackd2-dev libfreetype6-dev \
    libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev \
    libxrender-dev libglu1-mesa-dev mesa-common-dev
  ```

- **macOS universal binaries:** add `-G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13`.
- **Without CLAP:** add `-DAP_CLAP=OFF`.
- **Measurement tools:** leave out `-DAP_TOOLS=OFF` to also build the hosts for the reference plug-ins (`tools/`) and
  each plug-in's offline renderer, editor snapshot and fitting helpers.

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
| `docs/` | branding and screenshots |
| `.github/workflows/release.yml` | builds, tests and validates every plug-in on all three systems (pluginval at strictness 10, auval on macOS), makes the installers and, on a `v*` tag, publishes a release with the notes from [CHANGELOG.md](CHANGELOG.md) |

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
