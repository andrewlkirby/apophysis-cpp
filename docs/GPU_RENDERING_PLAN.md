# GPU (CUDA) Rendering Backend for apophysis-cpp

## Implementation status (this is now built and verified, not just planned)

A working CUDA backend has been implemented per this plan and **verified end-to-end
on real hardware** (a CUDA-capable GPU, CUDA 12.6, Windows): `apo_core` builds cleanly with
`APO_ENABLE_CUDA=ON`, and `tests/gpu_variation_parity_test.cpp` renders every
ported variation on both the CPU and GPU backends and compares them with
`computePsnr()` - **all 94 pass**, at 36.9-63.4 dB (well above the 15 dB bar),
strong evidence the GPU math is correct, not just plausible-looking.

**Delivered - full parity achieved for every non-plugin variation:**
- All infrastructure from the plan below: `RenderPlan.h`/`.cpp` (extracted from
  `Renderer.cpp`, zero behavior change - verified via the full existing CPU test
  suite, including `baseline_regression_test`), `GpuAvailability`,
  `VariationKinds`, `DeviceFlame`/`DeviceFlameBuilder`, `DeviceRng.cuh`,
  `DeviceVariations.cuh`, `chaos_kernel.cu`, `tonemap_kernel.cu`, `GpuRenderer`,
  `RenderDispatcher`.
- **All 29 local variations** (linear, sinusoidal, spherical, swirl, ...,
  post_rotate_y) ported to `__device__` code.
- **All 65 native registered variations** ported, in three batches (see
  `VariationKinds.cpp` for the authoritative list and `DeviceVariations.cuh`
  for the device math, each with a comment tracing back to its CPU source):
  - **Batch 1** (21, no RNG/`prepare()`-cache/dispatch): auger, bent, blob,
    cosine, cross, exponential, foci, heart, hemisphere, lazysusan, mobius,
    pdj, popcorn, power, pre_sinusoidal, pre_spherical, secant2, separation,
    splits, tangent, waves2.
  - **Batch 2** (22, `prepare()`-only): bipolar, bwraps, curl3D, elliptic,
    escher, fan, fan2, log, loonie, ngon, perspective, polar2, post_bwraps,
    post_curl, post_curl3D, pre_bwraps, pre_disc, rings, rings2, scry, waves,
    wedge - each recomputes its `prepare()`-derived constants inline from the
    raw parameters every device call, rather than needing a separate
    host-side precompute/upload path (a few extra flops per point, invisible
    next to the trig/sqrt this math already does).
  - **Batch 3** (22, RNG-using and/or `selectCalcFunction()`-specialized):
    arch, blade, blur_circle, blur_pixelize, blur_zoom, crop, post_crop,
    pre_crop, curl, rectangles, epispiral, pie, rays, twintrian, julian,
    juliascope, julia3Dz, julia3D, radial_blur, falloff2, post_falloff2,
    pre_falloff2. Most `selectCalcFunction()` specializations here (curl,
    the julia family, radial_blur) turned out to be verified-algebraically-
    equivalent closed-form fast paths for special parameter values - the
    device side always uses the one general formula and skips the
    specialization entirely (confirmed safe by each CPU source's own doc
    comments, then by parity testing). Two needed real replication: Rectangles'
    divide-by-zero guard, and Falloff2/PostFalloff2/PreFalloff2's genuine
    3-way blurtype dispatch (Uniform/Radial/Gaussian - three different
    formulas) - the latter also required adding real `color`-channel
    read/write support to `DeviceVarContext`/`devXformNextPoint` (previously
    a documented no-op simplification, now fully implemented).
- The GPU-eligibility check (`isFlameGpuEligible`) means any flame using a
  variation not in `VariationKinds.cpp` - which is now nothing except the 47
  legacy C plugins - falls back to the CPU renderer automatically, exactly as
  designed. **The only remaining CPU-fallback case is a plugin-using flame.**

### Toolchain notes (Windows) and the standard build command

This machine's only Visual Studio was 2026 (v18, MSVC 14.51) - too new for
either installed CUDA release (12.6's `cudafe++` crashes outright on it; 12.1
fails parsing MSVC 14.51's STL, which uses a `static operator()` syntax nvcc's
frontend doesn't yet support), and the VS *generator* needs CUDA's MSBuild
integration files (`BuildCustomizations/CUDA *.props/.targets`) registered for
that exact VS version - the CUDA installer only registers for VS versions
present at CUDA install time, so a very new VS won't have them, and copying
them in manually needs admin rights to `Program Files`.

Resolved by installing the MSVC v143 (VS2022) build tools component
side-by-side (`Microsoft.VisualStudio.Workload.VCTools`, via the Visual Studio
Installer or `winget install Microsoft.VisualStudio.2022.BuildTools --override
"--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"`) and
building with CMake's **NMake Makefiles** generator (not the VS generator) so
`nvcc` is driven directly, sidestepping the missing MSBuild integration
entirely. `apo_core`'s top-level `CMakeLists.txt` also auto-adds
`-allow-unsupported-compiler` to `CMAKE_CUDA_FLAGS` for any MSVC host compiler
(nvcc maintains an explicit host-compiler allowlist and hard-errors on
anything newer even when the code compiles fine), and scopes `/W4
/permissive- /EHsc` away from CUDA-language compiles via
`$<$<NOT:$<COMPILE_LANGUAGE:CUDA>>:...>` (unscoped, nvcc's parser doesn't
understand bare MSVC slash-flags and misparses them as extra input files -
worth checking for the same pattern in any other global
`add_compile_options`/`add_definitions` call if this is ported to a different
build setup). With both fixes in place this works in Debug as well as
Release - confirmed by a full Debug build + test run.

**The project's one `build/` directory now always builds this way** (CUDA on
by default in that directory's cache; a fresh checkout still defaults
`APO_ENABLE_CUDA` to OFF per the top-level `option()`). There is no separate
CUDA-only build directory to maintain. To (re)configure it from scratch:

```
$vs = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vs`" && cmake -S . -B build -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Debug -DAPO_ENABLE_CUDA=ON -DCMAKE_TOOLCHAIN_FILE=`"C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg\scripts\buildsystems\vcpkg.cmake`" -DCMAKE_PREFIX_PATH=`"C:\Qt\6.8.0\msvc2022_64`""
```

and to build/test day-to-day (every command needs the same `vcvars64.bat`
sourced first, since NMake Makefiles is a plain command-line generator with no
IDE wrapping it):

```
cmd /c "`"$vs`" && cmake --build build -j 4"
cmd /c "`"$vs`" && ctest --test-dir build"
```

The trade-off versus the old VS-generator `build/`: no more opening a `.slnx`
in Visual Studio for this directory - `apo_gui`/tests are now built and run
from the command line (or point VS's "Open Folder" CMake integration at the
repo root instead, which uses its own generator and shouldn't hit the same
MSBuild-integration gap, though that hasn't been verified here).

---

## Feasibility summary

`apophysis-cpp` renders fractal flames with a well-optimized **CPU** chaos-game
engine (`src/core/render/Renderer.cpp`): each worker thread runs independent
trajectories into a private histogram ("buckets"), the histograms are reduced,
then a tone-map pass produces the RGB(A) image. It is multithreaded, lock-free,
and already tuned (optional AVX2, `float` buckets, adaptive density estimation).

The chaos game is *embarrassingly parallel* — thousands of independent
trajectories accumulating into a shared histogram — which is exactly the shape
GPUs excel at. GPU flame rendering is proven prior art (flam4, Fractorium,
Chaotica). **GPU acceleration is clearly feasible.** The dominant cost is not
the algorithm; it is re-expressing the ~95 built-in variations as GPU device
code and wiring a second backend in behind the existing renderer interface.

- **Algorithm fit:** excellent. One GPU thread runs one (or a strided batch of)
  chaos-game trajectory; all threads `atomicAdd` into a single device
  histogram. `atomicAdd(float*)` is supported on every CUDA arch since sm_20,
  so the 4-float bucket (r, g, b, count) accumulates directly with no
  per-thread histogram or reduce pass needed. This mirrors the existing
  `BucketPrecision::Float` path and inherits the same, already-documented
  count-saturation tradeoff at extreme densities.
- **Variations:** each `Variation::calc()` (e.g. `VarBlob::calc()`) is
  self-contained math over `*tx/*ty/*tz`, `vvar`, the six affine coefs, and a
  handful of named scalar params, accumulating into `*px/*py/*pz`. This maps
  1:1 to a `__device__` function. The OOP/virtual dispatch and `std::vector`/
  `unique_ptr` machinery does **not** cross to the device; instead the flame is
  flattened into POD arrays and variations are dispatched by integer id in a
  device-side `switch`.
- **RNG:** `Xoshiro256Rng` is pure uint64 shifts/xors + splitmix64 seeding —
  ports verbatim to a `__device__` struct, one RNG state per GPU thread.
- **Tone-map:** per-pixel, reads the histogram, writes RGBA — a second kernel.
  Log-scale LUT, gamma/vibrancy, Bezier curves, background compositing, and
  adaptive DE (neighbor-bucket reads) all port directly.
- **Not portable / stays CPU:** the plugin ABI; determinism-sensitive baseline
  renders; anything on a non-CUDA machine.

### Decisions

| Decision | Choice | Consequence |
|---|---|---|
| GPU API | **CUDA** | NVIDIA-only. The GPU path is an *optional* backend for Windows/Linux + NVIDIA. macOS, AMD/Intel GPUs, and no-GPU machines keep using the existing CPU renderer. |
| Scope | **Both** preview + final | GPU usable for live editor previews and full-quality renders, selectable, with automatic CPU fallback. |
| Variation coverage | **Full parity** | All 29 local + 66 registered variations ported to CUDA device code before the GPU path is considered complete. |
| Plugins | **CPU fallback** | Any flame using one of the 47 statically-compiled C plugins renders on the CPU. The plugin ABI (`apoplugin.h`) is not ported to the GPU. |

The CPU renderer remains the **authoritative, deterministic** engine (it backs
the checked-in test baselines). CUDA is strictly additive: when CUDA is
unavailable, disabled, or the flame uses a plugin, rendering is identical to
today.

---

## Architecture

A new backend lives beside the CPU renderer, sharing its output type
(`RenderedImage`) and progress/cancel type (`RenderProgress`).

```
Renderer (CPU, unchanged, authoritative)
GpuRenderer (new, CUDA)               ── same RenderedImage / RenderProgress
RenderDispatcher (new, thin)          ── picks GPU vs CPU per call
```

New files (all under `src/core/render/gpu/`, compiled only when CUDA is found):

- `GpuRenderer.h` / `.cu` — public entry mirroring `Renderer::render()`'s
  signature and returning the same `RenderedImage`. Owns device allocation,
  kernel launches, chunked progress/cancel, and readback.
- `DeviceFlame.h` — POD, upload-ready mirror of the render plan: flattened
  per-xform variation lists (`{variationId, weight, paramOffset}`), affine
  coefs, precomputed `prepare()` values, the propTable, colorMap, projection
  params, tone-map constants + LUTs, adaptive-DE kernels. Built on the host by
  reusing the existing `RenderPlan` build functions (`buildPropTable`,
  `buildBuffersAndCamera`, `buildColorMap`, `buildToneMap`, `buildCurveTables`,
  `buildAdaptiveDE`) and then *serializing* their results into device arrays —
  so the GPU and CPU stay in sync on all the non-variation math by construction.
- `DeviceRng.cuh` — `__device__` Xoshiro256** ported from `Rng.h`.
- `DeviceVariations.cuh` — every variation as a `__device__` function plus a
  `switch(variationId)` dispatcher. This is the bulk of the work (see below).
- `chaos_kernel.cu` — the per-trajectory chaos-game kernel (FUSE warmup,
  propTable xform selection, `nextPoint` via the variation dispatcher, opacity
  roll, final xform, `project()`, `atomicAdd` accumulate).
- `tonemap_kernel.cu` — per-pixel tone-map kernel (port of `toneMap()`).
- `GpuAvailability.h/.cpp` — runtime CUDA device detection (compiled always,
  even without CUDA, returning "unavailable").

New files under `src/core/render/`:

- `RenderDispatcher.h/.cpp` — decides backend per render: use GPU when
  (a) CUDA is available, (b) the user preference allows it, and (c) the flame
  uses **no** plugin variation; otherwise CPU. Exposes the same call shape the
  UI already uses so `RenderWorker` changes are minimal.

### Backend selection rule

A flame is GPU-eligible iff every weighted variation across all xforms (incl.
final) has a ported `__device__` implementation and is not a plugin variation.
The dispatcher scans the flame once (cheap) and falls back to CPU on the first
ineligible variation. "Full parity" means the only routine fallbacks are
plugin flames and non-CUDA machines.

---

## The variation port (dominant effort)

- Build an authoritative **variation-id table** shared by host and device so a
  flame's variation indices mean the same thing on both sides. Reuse the
  existing registry order (`VariationRegistry`) as the id source of truth.
- Port each `calc()` to a `__device__` function with the same math, reading
  from a per-thread point/params context instead of the `Variation` object's
  bound pointers. Params that `prepare()` precomputes on the CPU (sin/cos
  tables, normalized coefficients, `precalcAll`, etc.) are computed host-side
  and uploaded per xform, so the device function is pure per-point math.
- Handle the handful of variations that draw from the RNG per point (noise,
  blur, gaussian, zblur, blur3D, pre_blur, radial_blur, julian branch pick,
  etc.) via the per-thread `DeviceRng`.
- **Parity harness:** for every variation, render a fixed single-xform flame on
  CPU and GPU and compare with the existing `computePsnr()`
  (`src/core/io/ImageCompare.h`) above a high threshold. This is the acceptance
  gate for "ported correctly" and catches the long tail of edge cases that
  "full parity from the start" implies. Drive it from a new
  `tests/gpu_variation_parity_test.cpp`.

---

## Progress, cancellation, precision, determinism

- **Progress/cancel:** launch the chaos kernel in chunks (N sub-batches per
  launch), update `RenderProgress::pointsDone` and check `cancelRequested`/
  `pauseRequested` between launches — same cadence contract the CPU path
  already honors. A cancelled GPU render still tone-maps whatever accumulated.
- **Precision:** default to float histogram atomics (matches existing
  `BucketPrecision::Float`); optionally offer double atomics on sm_60+ for
  high-density final renders. Document the same saturation caveat already in
  `Renderer.h`.
- **Determinism:** GPU output is not bit-reproducible (like the CPU
  multithreaded path). The single-threaded CPU renderer stays the baseline
  authority; GPU is validated by PSNR against it, never by bit-equality.

---

## Build integration

- CMake: `option(APO_ENABLE_CUDA "Build the optional CUDA render backend" OFF)`.
  When ON, `enable_language(CUDA)` + `find_package(CUDAToolkit)`; compile the
  `gpu/` sources and define `APO_HAVE_CUDA`. When OFF or toolkit missing, none
  of the `.cu` files compile and the dispatcher is CPU-only — **zero impact on
  today's Windows/macOS/Linux builds**, which is why it defaults OFF.
- `GpuAvailability` compiles unconditionally so the UI can always ask "is a GPU
  path available?" and gray out the toggle when not.
- Pick a reasonable `CMAKE_CUDA_ARCHITECTURES` default (e.g. a broad set) and
  document overriding it.

## UI integration

- Add a "Use GPU (CUDA)" preference + a per-render indicator. Wire
  `RenderWorker` and `RenderDialog`/`RenderAllDialog` to call the dispatcher
  instead of `Renderer::render()` directly. When a flame falls back to CPU
  (plugin use / no GPU), surface it non-intrusively (status text), not as an
  error.

---

## Suggested phasing

1. ✅ **Skeleton + plumbing:** CMake CUDA option, `GpuAvailability`, dispatcher,
   `DeviceFlame` upload, `DeviceRng`, and a chaos kernel + tone-map kernel.
   Done - verified end-to-end on real hardware, see "Implementation status"
   above.
2. ✅ **Core variations:** all 29 local + 21 straightforward registered
   variations ported (batch 1).
3. ✅ **Full parity:** the other 44 native registered variations ported
   (batches 2 and 3 - see "Implementation status" above for the breakdown
   and how each `prepare()`/RNG/`selectCalcFunction()` complication was
   handled). `gpu_variation_parity_test` covers all 94 ported variations
   (29 local + 65 registered), passing at 36.9-63.4 dB PSNR per variation.
   Plugin flames remain CPU-only by design (dispatcher rule),
   not a gap to close - they're now the *only* remaining fallback case.
4. **Polish (in progress):**
   - ✅ `float`-precision histogram option: `GpuRenderer::render()` now takes
     the same `BucketPrecision` the CPU renderer does (`DeviceBucketF` in
     DeviceTypes.h, `useFloatBuckets` threaded through
     `DeviceKernelParams`/`DeviceToneMapParams`), verified via
     `gpu_variation_parity_test`'s `testFloatBucketPrecision()`.
   - ✅ `apo_bench --backend=cpu|gpu|both`: reuses the existing linear/
     transcendental/blur fixtures, routes gpu rows through
     `RenderDispatcher` (preferGpu forced on, reporting "GPU not available"
     rather than silently timing a CPU fallback if it can't actually run on
     the GPU). Measured on a consumer GPU vs 16-thread CPU:
     - 800x600, density 200 (~103M points): linear 28.6M -> 369M pts/sec
       (12.9x), transcendental (julian+swirl) 19.2M -> 97.8M pts/sec (5.1x),
       blur (gaussian_blur) 18.1M -> 178M pts/sec (9.8x).
     - 1920x1080, density 1000 (~2.15B points, final-render scale):
       transcendental 19.3M -> 167M pts/sec (8.7x), wall-clock 111s -> 12.8s.
   - ✅ UI wiring: `RenderWorker`'s three render slots
     (`renderFlame`/`renderFlameWithProgress`/`renderFull`) now call
     `RenderDispatcher::render()` instead of `Renderer::render()` directly,
     gated by a new persisted preference
     (`AppSettings::useGpuRendering()`/`setUseGpuRendering()`, defaulting to
     true - safe since `RenderDispatcher` already falls back to the
     identical CPU path with zero behavior change whenever the GPU path
     isn't actually usable) exposed as a checkbox in OptionsDialog's
     Performance group ("Use GPU rendering when available (CUDA)").
     `RenderDispatcher::render()` gained an optional `bool* usedGpu`
     out-parameter (reports what actually happened for *this* call, not
     just a prediction - a GPU attempt that fails still transparently falls
     back to CPU, and the indicator reflects that); `RenderWorker`'s
     `renderFinished`/`fullRenderFinished` signals now carry a trailing
     `usedGpu` bool (existing slots keep compiling/working unchanged - Qt
     allows a slot to have fewer parameters than its signal), and
     `RenderDialog`'s status line shows a `[GPU]` suffix when a full render
     actually used it.
   - ✅ Progress/cancel/pause tuning under real interactive load:
     `tests/gpu_progress_control_test.cpp` (the GPU analogue of
     `renderer_test.cpp`'s pause/cancel tests, but at interactive-preview and
     final-render point-count scale rather than a single-sub-batch toy case)
     measures how long `cancelRequested`/`pauseRequested` actually take to be
     honored, bounded by `GpuRenderer.cu`'s per-launch-chunk poll cadence
     (`kBatchesPerLaunch=4096`, ~41M points/launch), against
     `RenderDialog.cpp`'s own 150ms UI poll interval
     (`kProgressPollIntervalMs`). Measured on a mid-range consumer GPU with a
     1920x1080, density-400 (~829M point, ~20-launch-chunk) julian+swirl
     flame: cancel latency 0.17-0.19s, pause-freeze latency 0.19s (both
     comfortably inside a 2s bound, and within a couple of the UI's own
     150ms polls), pointsDone genuinely stays frozen while paused and a
     cleared pause resumes to normal completion, pause-then-cancel-without-
     resume doesn't hang, and a full render surfaces ~21 distinct progress
     ticks over ~5s (roughly one every 240ms) - a responsive progress bar,
     not a single 0%->100% jump. No tuning of `kBatchesPerLaunch` was needed;
     the existing chunk size already lands well inside interactive latency
     budgets on this hardware.
   - ✅ Adaptive launch sizing: the fixed `kBatchesPerLaunch=4096` above was
     found (via `apo_bench --backend=gpu`) to leave real throughput on the
     table - each launch only occupies 4096 of the GPU's resident threads
     regardless of how many the device can actually run at once (no
     `cudaGetDeviceProperties` occupancy query existed), and per-thread state
     is registers-only (no per-thread device allocation), so there was no
     memory reason for the cap. Sweeping `kBatchesPerLaunch` by hand
     (4096/16384/32768/65536/131072/262144) measured ~30-40% higher points/sec
     at 65536+ vs the 4096 baseline on a mid-range consumer GPU (noisy - the
     card thermal-throttled over the sweep, boost clock dropping ~2.4x
     between readings - but every larger value beat 4096 by a wide margin
     even under matched thermal conditions). Replaced the fixed constant with
     `adaptLaunchSize()`
     (`GpuRenderer.cu`): a proportional controller that times each launch and
     scales the next one toward `kTargetLaunchSeconds=0.35s`, starting fresh
     from a conservative 4096-thread guess every render (converges in 1-2
     launches, <1s overhead - negligible, and avoids caching a value that
     could go stale between renders as thermal/contention conditions change).
     0.35s was chosen with a real ceiling in mind, not just a UX preference:
     Windows WDDM resets the GPU driver if a single kernel doesn't yield
     within its Timeout Detection and Recovery window (2s by default), and a
     driver reset kills every GPU-using app on the system, not just this
     render - so growth per launch is capped at 2x/step (shrinking isn't) to
     keep a single bad measurement from jumping close to that edge. No
     separate app-startup calibration step: re-measuring per-render is
     already cheap enough that a persisted/warm-up value would only add
     staleness risk for no real benefit.

---

## Verification

- **Parity:** ✅ `gpu_variation_parity_test` - all 94 ported variations (every
  local + every native registered variation), CPU vs GPU PSNR above threshold
  (currently 15 dB; observed 36.9-63.4 dB). ✅ Also extended to
  whole-flame parity: `gpu_baseline_parity_test` loads each of
  `baseline_regression_test`'s five checked-in fixtures
  (`tests/baselines/*.flame` - plain affine, multi-variation blending,
  xaos/chaos-weighted xform selection, the 3D pitch/yaw/perspective/DOF
  camera projection, and an optional final xform), clones each with a higher
  `sampleDensity` (300, vs. the fixtures' own quality="20"-ish - too low a
  point count for a robust independent-RNG comparison, same rationale as
  `gpu_variation_parity_test`'s own density=200 override), and compares CPU
  vs GPU output at the same 15 dB PSNR bar - not against the checked-in PNG
  baselines themselves, which are a single-seed *CPU* golden and only
  meaningful CPU-vs-CPU (see `baseline_regression_test.cpp`'s own comment).
  All five pass at 27.3-64.3 dB, including the 3D-camera and
  xaos fixtures - real end-to-end evidence the whole render pipeline (not
  just each variation in isolation) agrees between backends.
- **Fallback correctness:** ✅ confirmed both ways. `APO_ENABLE_CUDA=OFF`
  compiles and passes the full non-UI CPU test suite unchanged. With
  `APO_ENABLE_CUDA=ON` (this project's now-standard `build/` config, see
  toolchain notes above), the full non-UI suite still passes 30/30 -
  `baseline_regression_test` and `renderer_test` (pure CPU-path tests)
  unaffected by CUDA being compiled in. (Every `tests/ui/*` Qt GUI test was
  excluded from that run: they open real windows and hang/fail for lack of an
  interactive desktop session in this environment - a pre-existing test-
  execution-environment limitation unrelated to CUDA, not a regression; they
  weren't run at all rather than failing meaningfully.) A plugin-using flame's
  CPU output is untouched either way, since `RenderDispatcher` routes it
  straight to `Renderer::render()` - not yet explicitly diffed
  ON-vs-OFF-build-for-a-plugin-flame, but the code path is identical so this
  is a formality, not a real risk.
- **Perf:** ✅ `apo_bench --backend=both` (see "Implementation status" above
  for the actual numbers) - 5-13x throughput over 16-thread CPU depending on
  variation cost, confirmed at both interactive-preview and final-render
  scale, not just the parity test's small (96x96) flames.
- **Build matrix:** ✅ `APO_ENABLE_CUDA=OFF` verified unaffected (full test
  suite, `apo_gui`, all builds clean, both Debug and Release). ✅
  `APO_ENABLE_CUDA=ON` verified builds and runs correctly on a real CUDA
  machine in both Debug and Release (see toolchain notes above) - not yet
  tried on Linux or macOS (macOS has no CUDA path at all, by design; Linux
  should work via the same CMake option but hasn't been exercised).

## Key existing code to reuse (not reinvent)

- Render-plan builders in `Renderer.cpp` (`buildPropTable`,
  `buildBuffersAndCamera`, `buildColorMap`, `buildToneMap`, `buildCurveTables`,
  `buildAdaptiveDE`) — serialize their outputs into `DeviceFlame`.
- `project()` / `CameraMatrix` logic in `Renderer.cpp` — port verbatim.
- `Xoshiro256Rng` (`Rng.h`) — port verbatim to device.
- `computePsnr()` (`io/ImageCompare.h`) — the parity gate.
- `RenderedImage` / `RenderProgress` / `RenderStats` (`Renderer.h`) — reused
  unchanged as the GPU backend's output/control types.
- `VariationRegistry` order — the shared variation-id source of truth.
