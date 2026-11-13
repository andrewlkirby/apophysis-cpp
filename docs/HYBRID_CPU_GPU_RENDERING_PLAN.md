# Hybrid CPU + GPU rendering

**Status (2026-11-12): implemented** with the point-streaming design below. Results are in the progress log at the top of `docs/FOLLOWUP_PLAN.txt`. Default-on since 2026-11-13: on real multi-xform flames the GPU suffers warp divergence and hybrid is ~6x faster than GPU-only, while simple flames lose at most ~5-10% (see the 2026-11-13 follow-up in the log).

## Context

Today `RenderDispatcher::render()` picks exactly one backend: the CUDA `GpuRenderer` when the flame is eligible and large enough, otherwise the multithreaded CPU `Renderer`. During a GPU render the 16 CPU threads sit idle. The goal is to have both iterate points for the same render at the same time.

It is possible because:

- Both backends split a render into the same independent unit: `numBatches` sub-batches of 10,000 points (`kSubBatchSize` / `kDeviceSubBatchSize`), each with a fresh start point and FUSE warm-up. Any sub-batch can run on either backend.
- Histogram accumulation is a plain order-independent sum, and both backends build camera/bucket geometry from the same `RenderPlan` code and share `RenderProgress` (progress, cancel, pause).

Output is not bit-reproducible in hybrid mode (same as the existing multithreaded CPU path). Pure-GPU and pure-CPU paths stay unchanged; hybrid is opt-in.

## Step 0 results (2026-11-12, Ryzen 7 5800H 16 threads vs the dev machine's CUDA GPU)

CPU throughput as a share of GPU throughput:

| Fixture | 800x600 d200 double | 800x600 d200 float | 1920x1080 os2 d100 double |
|---|---|---|---|
| linear | 24% | 20% | 8% |
| transcendental | 42% | 26% | 21% |
| blur | 13% | 18% | 16% |

The originally planned merge (CPU threads keep private histograms, reduce, upload, add on device) does not pay off: at 1920x1080 os2 the CPU reduce alone is 0.2-0.75 s while the whole GPU render is 0.33-0.93 s. Hybrid would be slower than GPU-only on large or cheap-per-point renders.

## Design: CPU threads stream points to the GPU

CPU workers do not keep histograms. Each accepted point becomes an 8-byte record (`bucket index`, `color index`) written into a chunk of pinned host memory. The GPU driver thread uploads full chunks on a second CUDA stream and runs a small scatter kernel that adds them into the device histogram with the same `atomicAdd` math as the chaos kernel. At ~50M CPU pts/s this is ~400 MB/s over PCIe, which is easy. There is no CPU reduce and no per-thread bucket memory, so the gain should be close to the CPU's share above at every size.

```
shared std::atomic<int> nextBatch  (sub-batch claims, both backends)
 ├─ CPU producer: N-1 worker threads → renderBatch with a stream sink
 │     → PointStream chunk pool (pinned) → full-chunk queue
 └─ GPU driver thread: chaos launches (claims a launch's worth at a time)
       while a launch runs: pump queue → async upload + scatterKernel on side stream
       when all batches claimed: join CPU, drain queue → existing tonemapKernel
```

## Steps

### 1. CPU producer (`src/core/render/Renderer.cpp`, new `src/core/render/PointStream.h`, new `src/core/render/CpuPointProducer.h`)

- `PointStream.h`: `StreamedPoint {uint32 bucket; uint32 color}`; a mutex/condvar chunk pool over caller-owned storage: `acquireEmpty()` (blocks, returns null after `abort()`), `submitFull()`, `tryPopFull()`, `release()`, `abort()`.
- `renderBatch` is templated on a sink called where `accumulate` was; the bucket path passes a lambda that calls `accumulate`, so the CPU-only path is unchanged (baseline tests must stay bit-exact). Per-thread setup moves into a shared helper used by both `renderImpl` and the producer.
- `CpuPointProducer`: starts workers that claim sub-batches from the shared counter and write records into chunks; `join()` flushes partial chunks, returns `RenderStats`, rethrows the first worker exception. CPU per-thread seeds are salted (`seed + (2^32 + t) * golden`) so they can't collide with GPU per-sub-batch seeds (`seed + idx * golden`, idx < 2^31).

### 2. GPU side (`src/core/render/gpu/GpuRenderer.cu/.h`, `chaos_kernel.cu/.cuh`)

- `GpuRenderer::render(..., int cpuAssistThreads = 0)`; 0 is today's code path.
- Launch loop claims from the shared counter via `fetch_add`, clamped to `numBatches`. While a chaos launch runs, poll `cudaStreamQuery` and pump the point queue (async upload to one of a few device staging slots + `scatterKernel` on a non-blocking stream; recycle host chunks when their event completes).
- After the loop: `join()` the producer, drain remaining chunks, sync, add CPU stats, then tone-map exactly as today. On any CUDA failure, `abort()` the stream, join, return empty (dispatcher falls back to CPU).

### 3. Orchestration (`src/core/render/RenderDispatcher.cpp/.h`)

- New trailing parameter `bool cpuAssist = false` on `render()`; CPU threads = `threadCount - 1` (auto = `hardware_concurrency() - 1`; the calling thread drives CUDA).
- `wouldUseCpuAssist(flame, preferGpu, cpuAssist)` for the UI label.

### 4. UI and settings (`src/ui/AppSettings.*`, `OptionsDialog.cpp`, `RenderWorker.h`, `RenderDialog.cpp`)

- `AppSettings::useCpuAssist()` (`rendering/cpuAssist`), checkbox "Also use CPU threads during GPU renders" under the GPU checkbox.
- Final renders (`RenderWorker::renderFull`, batch render) pass it through; previews stay as they are.
- RenderDialog backend label: "GPU + CPU".

### 5. Bench and tests

- `apo_bench --backend=hybrid` (and included in `both`).
- New `tests/hybrid_render_test.cpp` (skips without CUDA): PSNR vs GPU-only above `gpu_baseline_parity_test`'s bar; `pointsGenerated == numBatches * 10000` for a well-behaved flame (no sub-batch lost or doubled); CPU actually contributed; cancel returns a non-empty image; pause/resume completes; float and double.
- Dated entry in `docs/FOLLOWUP_PLAN.txt`'s progress log.

## Verification

1. `cmake --build build-release`, then `ctest --test-dir build-release --output-on-failure`: all existing tests pass with no baseline regeneration, plus the new hybrid test.
2. `apo_bench --backend=gpu` vs `--backend=hybrid` on linear/transcendental/blur at 800x600 d200 and 1920x1080 os2 d100, double and float. If hybrid is slower than GPU-only on any fixture, keep the option default-off.
3. GUI: enable the option, run a RenderDialog final render to PNG; check label, progress, Pause, Cancel.

## Risks

- **Scatter kernel competes with the chaos kernel for GPU time.** Each CPU point costs the GPU a few atomics; for atomic-bound flames (linear) the net gain shrinks. Measured in step 5.
- **CPU contention with the CUDA driver thread.** One core is left for it.
- **Pinned memory.** The chunk pool is ~50 MB of pinned host memory per hybrid render, allocated once per render.
