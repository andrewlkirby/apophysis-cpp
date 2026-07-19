# Apophysis 7X — C++ Port

A modern C++20/Qt6 port of [Apophysis 7X](https://sourceforge.net/projects/apophysis7x/),
the classic fractal-flame editor. This is a ground-up reimplementation of the
original Delphi/VCL application — same rendering algorithm and variation
library, new codebase — built for better performance and long-term
maintainability, not a wrapper or a patch on top of the original.

> **Status:** actively developed. Core rendering, flame editing, and the
> majority of the original UI have been ported; see
> [`docs/MISSING_FEATURES_PLAN.txt`](docs/MISSING_FEATURES_PLAN.txt) and
> [`docs/FOLLOWUP_PLAN.txt`](docs/FOLLOWUP_PLAN.txt) for the detailed,
> ongoing audit of what's been ported and what's left. Builds and runs on
> Windows, Linux, and macOS, verified by CI on every push
> (`.github/workflows/ci.yml`); the packaged installer/`.zip` distribution
> under [Packaging](#packaging-a-standalone-build) is Windows-only for now.

See [`docs/USAGE.md`](docs/USAGE.md) for a guide to using the app itself —
the library window, the flame editor, rendering, gradients, and the
randomization tools.

## Features

- **Chaos-game renderer** — multithreaded, batch-based, with adaptive
  (density-estimation) filtering, gamma/vibrancy/brightness color
  correction, transparent-background export, and cooperative
  cancel/pause/resume mid-render.
- **~90 variations** ported from flam3/Apophysis, including the full local
  (parameterless) set and dozens of parametric variations, plus a
  compatibility shim for legacy third-party plugin variations (`julia`,
  `fisheye`, etc.).
- **Full flame editor** — triangle-based affine transform editing with live
  canvas drag, post-transforms, xaos, per-transform color/variation/variable
  tabs, symmetry tools, and undo/redo.
- **Gradient tools** — gradient browser/library, editing (rotate, hue,
  saturation, brightness, contrast, blur, frequency, invert, reverse), and
  smooth-palette generation.
- **Random flame / mutation tools** — configurable random batch generation
  (including variation blending and per-variation parameter randomization),
  a Mutate dialog for exploring nearby variants, and flame interpolation.
- **Render pipeline** — a render dialog for full-quality PNG export (with
  pause/resume), batch "Render All" for a whole library, and a headless CLI
  (`apo_render_cli`) for scripted rendering.
- **File compatibility** — reads/writes the original `.flame` (XML) and
  `.ugr`/gradient library formats.

## Getting started

### Prerequisites

- **CMake** 3.20+
- **A C++20 compiler** — MSVC (Visual Studio 2022+), GCC, or Clang; all
  three are exercised in CI (Windows/MSVC, Linux/GCC via Ninja, macOS/Clang
  via Ninja).
- **[vcpkg](https://github.com/microsoft/vcpkg)**, for `pugixml` and
  `libpng` (pulled in automatically via `vcpkg.json` manifest mode — no
  manual `vcpkg install` needed, just point CMake at your vcpkg toolchain
  file).
- **Qt 6** (Widgets + Test components — both included in a base Qt6
  install), e.g. via a prebuilt SDK:
  ```
  pip install aqtinstall
  # Windows
  python -m aqt install-qt windows desktop 6.8.0 win64_msvc2022_64 -O C:\Qt
  # Linux
  python -m aqt install-qt linux desktop 6.8.0 linux_gcc_64 -O ~/Qt
  # macOS
  python -m aqt install-qt mac desktop 6.8.0 clang_64 -O ~/Qt
  ```
  Building Qt from source via vcpkg also works but takes considerably
  longer; a prebuilt SDK is the faster path.
  On Linux, running the app or its offscreen Qt tests also needs the usual
  Qt runtime libraries on the system (GL, xkbcommon, fontconfig, etc.) —
  see `.github/workflows/ci.yml` for the exact `apt-get install` list CI
  uses.

### Build

**Windows** (Visual Studio's generator is multi-config, so the build type is
chosen at build time, not configure time):
```
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg root>/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
cmake --build build --config Release
```
The GUI app builds as `build/src/ui/Release/apo_gui.exe`.

**Linux / macOS** (Ninja/Makefiles are single-config generators, so the
build type is chosen at configure time instead):
```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=<vcpkg root>/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=~/Qt/6.8.0/gcc_64
cmake --build build
```
The GUI app builds as `build/src/ui/apo_gui` (no `Release/` subdirectory).
Use `clang_64` in place of `gcc_64` for the `CMAKE_PREFIX_PATH` on macOS.

Optional: enable AVX2 codegen for the render/variation hot path (raises the
minimum supported CPU to roughly Haswell/2013+) with
`-DAPO_CORE_ENABLE_AVX2=ON`.

### Run the tests

```
ctest --test-dir build -C Release   # Windows (multi-config generator)
ctest --test-dir build              # Linux/macOS (single-config generator)
```

Tests are split into pure-logic tests (no Qt widgets, `tests/*_test.cpp`)
and real-widget Qt interaction tests (`tests/ui/*_test.cpp`, simulated
mouse/keyboard events against actual compiled widgets, run with
`QT_QPA_PLATFORM=offscreen`).

### Headless rendering

```
apo_render_cli <input.flame> <output.png> [--seed=N] [--threads=N] [--width=W] [--height=H] [--index=N]
```

### Packaging a standalone build

```
cmake --build build --config Release --target package_zip
```

Produces `build/deploy/` (a self-contained, redistributable copy of the app
— Qt and MSVC runtime DLLs included) and
`build/apophysis7x-<version>-win64.zip`. An Inno Setup installer script is
also provided at `packaging/apophysis7x.iss` (`iscc packaging\apophysis7x.iss`,
run from the repo root, after the `deploy` step above).

## Project layout

```
src/core/       Qt-free rendering engine, flame/variation data model, file I/O
src/core/variations/  Individual variation implementations
src/core/edit/  Pure mutation/randomization primitives (shared by UI and CLI)
src/core/render/       The chaos-game renderer
src/ui/         Qt6 Widgets application (MainWindow, EditorWindow, dialogs)
src/tools/      apo_render_cli, apo_bench
tests/          Logic tests (Qt-free) and tests/ui/ (Qt widget interaction tests)
docs/           Design/planning notes and audit logs from the porting effort
packaging/      Inno Setup installer script
```

## License

GPL-3.0 — see [LICENSE](LICENSE), matching the original Apophysis 7X project.
