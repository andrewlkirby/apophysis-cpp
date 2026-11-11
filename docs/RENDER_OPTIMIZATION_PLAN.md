# Render-path optimization plan

**Status (2026-11-10): steps 1-4 implemented.** Results are in the progress log at the top of `docs/FOLLOWUP_PLAN.txt`. The "Not in this plan" items are still open.

## Context

The CPU renderer has already had a thorough pass (FOLLOWUP_PLAN.txt B0–B7: lean clones, CalcFn dispatch, concrete RNG, parallel reduce/tonemap, float buckets). I re-read the hot path and ran the existing `build-release\src\tools\apo_bench.exe` (Ryzen 7 5800H, 16 threads, MSVC 14.44 /O2). The chaos-game loop is in decent shape, but the measurements exposed a large defect *outside* it that the earlier work missed because it was benchmarked mostly single-threaded:

| Run | wall | setup | iterate | reduce |
|---|---|---|---|---|
| 800x600 d200, linear, 1 thread | 4.94s | 0.07s | 4.85s | 0 |
| 800x600 d200, linear, 16 threads | 2.18s | **1.13s** | 0.95s | 0.08s |
| 1920x1080 os2 d100, 16 threads | 25.4s | **20.0s** | 3.6s | **1.45s** |
| 1920x1080 os2, 1 thread, ~0 density, double | 1.32s | **1.22s** | – | – |
| same, float buckets | 0.13s | 0.03s | – | – |

Setup costs ~4.4 ms per MB of **double** bucket memory, per thread, serially on the calling thread. The same memory in float is ~40x faster, and a raw malloc+memset of the same size on this machine takes 2–7 ms per 16 MB (vs 70 ms here). So it is the element-wise `ts.buckets.assign(n, BucketT<double>{})` fill (Renderer.cpp:602) and the identical fresh-vector fill in `reduceBuckets` (Renderer.cpp:340), not page-fault cost. At 16 threads this is more than half of wall time for a typical render and ~80% for a large one.

Second finding: with GPU rendering enabled, live-preview-sized renders are slower on GPU than CPU (200x150 d30: GPU 46–114 ms vs CPU 17–20 ms) because a preview is only ~117 sub-batches = 117 GPU threads each grinding 10,000 sequential iterations.

Intended outcome: ~2x faster wall time for normal multi-threaded CPU renders, ~5x for large/oversampled ones, lower peak memory, faster GPU-mode previews, plus a few bit-identical per-point micro-wins. No rendered-output changes (baselines must stay PSNR=inf).

## Step 1 — Zero-cost bucket allocation (the big one)

File: `src/core/render/Renderer.cpp`

- Add a small internal RAII `BucketBuffer<Scalar>` (owning pointer + size, `operator[]`, `data()`, move-only) backed by `std::calloc`/`std::free`; throw `std::bad_alloc` on null so the existing worker/RenderWorker exception handling still applies. `static_assert(std::is_trivially_copyable_v<BucketT<Scalar>>)`; all-bits-zero is a valid zero bucket for float/double.
- Replace `std::vector<BucketT<Scalar>>` with it in `ThreadState`, `accumulate`, `reduceBuckets`, `safeGetBucket`, `toneMap`.
- Effect: fresh large blocks come back as untouched zero pages - no fill at all. First-touch then happens lazily inside each worker thread (parallel, and only for pages the attractor actually hits).

## Step 2 — Reduce in place

File: `src/core/render/Renderer.cpp`, `Renderer.h` (comments), `src/ui/MemoryBudget.*` only if its comments cite the formula

- `reduceBuckets`: sum sources 1..N-1 into `threadStates[0].buckets` and move it out, instead of allocating+filling a new array. Per bucket the addition order stays `b0 + b1 + ... + bN-1`, so output is bit-identical. Keep the `parallelFor` split and the `numSources == 1` move.
- `Renderer::estimatePeakMemoryBytes`: `(threads + 1)` -> `threads` (the +1 array no longer exists). This also lets `resolveMemorySafeThreadCount` allow one more thread on tight budgets.
- Update tests that encode the old formula: `tests/ui/memory_budget_test.cpp` (~line 45-61 reasons about `threadCount+1`) and the estimate cases in `tests/renderer_test.cpp` (~400-530; most are relational and should pass unchanged).

## Step 3 — GPU: don't send tiny renders to the GPU

Files: `src/core/render/RenderDispatcher.cpp/.h`

- In `wouldUseGpu`, also require the render to be big enough: compute `numBatches` the same way the renderer does (`buildFilter` + `buildBuffersAndCamera` on a local `RenderPlan`, as `estimatePeakMemoryBytes` already does) and require `numBatches >= kMinGpuBatches`. Start at 1024 (~10M points; equals `kMinBatchesPerLaunch`) and tune with apo_bench at 200x150, 400x300, 800x600 low density.
- `cpuFallbackReason`: return "render too small to benefit from GPU" for that case so the GPU/CPU badge explains itself.
- Check `tests/gpu_*_test.cpp` and `apo_bench --backend=gpu` fixtures still exceed the threshold (or call `GpuRenderer::render` directly where a test deliberately uses a tiny flame).

## Step 4 — Per-point micro-optimizations (bit-identical, measure each)

Files: `src/core/render/Renderer.cpp`, `src/core/Rng.h`

Linear fixture is ~48 ns/point single-threaded, which is high for an affine map + one bucket add; the fixed per-point overhead is CRT calls MSVC does not inline.

- `accumulate()`: three `std::lround` calls per point. Replace with an inline helper that clamps in the double domain first, then `int i = (int)v; i += (v - i >= 0.5);` - exactly `lround` for the non-negative clamped range. Use the same helper in `logScaleFor` (called per filter tap in `toneMap`).
- `renderBatch()`: 8 `std::isfinite` calls per point -> one inline bit test helper (exponent-mask check via `std::bit_cast`), same semantics, no reliance on /fp mode.
- `Rng.h`: add `Xoshiro256Rng::uniformBits(int bits)` returning `next() >> (64 - bits)`; use it for the propTable draw (`kPropTableSize` = 1024 -> 10 bits). This equals `int(uniform01() * 1024)` bit-for-bit (top 10 of the same 53 bits), so the RNG stream and output are unchanged; it just skips the int->double->int round trip. Add a `static_assert` that `kPropTableSize` is a power of two.
- Hoist `plan.*` scalars and `ts.flame->xform` raw pointers into locals at the top of `renderBatch`.

Keep each as its own commit; drop any that apo_bench cannot distinguish from noise.

## Not in this plan (noted for later)

- Shared/smaller histogram to improve thread scaling (linear fixture scales only ~5x on 8 cores because 16 private arrays overflow L3). Real potential, but it changes the lock-free-private-bucket design and determinism story - worth its own experiment after steps 1-2 land.
- Persistent thread pool / buffer reuse across preview renders (saves a few ms of a ~17 ms preview).
- GPU kernel in single precision (large speedup on consumer GPUs, but changes output).

## Verification

1. Build: `cmake --build build-release` (NMake, Release, CUDA on).
2. `ctest --test-dir build-release --output-on-failure` - in particular `baseline_regression_test` must remain bit-exact (PSNR=inf) with no baseline regeneration, plus `renderer_test`, `memory_budget_test`, `gpu_*` tests.
3. Re-run the same benchmark commands as above and compare phase timings:
   - `apo_bench --threads=1,4,8,0` (expect 16-thread setup 1.13s -> ~0.01s, wall ~2.2s -> ~1.0s)
   - `apo_bench --threads=0 --size=1920x1080 --oversample=2 --density=100 --flame=transcendental` (expect ~25s -> ~4-5s)
   - `apo_bench --threads=1 --size=1920x1080 --oversample=2 --density=0.01 --flame=linear` (isolates setup)
   - `apo_bench --threads=0 --size=200x150 --density=30 --bucket-precision=float` with and without `--backend=gpu` (step 3 threshold tuning)
4. Launch the GUI, drag a transform in the editor with GPU enabled (preview should feel snappier and badge should show CPU for previews, GPU for full renders), and do one RenderDialog final render to PNG.
5. Add a dated entry with before/after numbers to the progress log at the top of `docs/FOLLOWUP_PLAN.txt`, per that file's convention.
