#pragma once

// Measures how fast the CPU and GPU backends iterate a specific flame at its
// real render size, so a caller (RenderDialog's "Auto" backend) can pick
// between GPU-only, GPU + CPU (hybrid) and CPU-only, and estimate how long
// the render will take.
//
// Why measure rather than predict from settings: which backend wins depends
// mostly on the flame itself (how expensive its variations are, how much
// its xforms diverge on the GPU), not on size/density. Measured on a laptop
// (docs/FOLLOWUP_PLAN.txt, 2026-11-13): a real 5-xform flame rendered ~2.3x
// faster in hybrid mode than GPU-only, with the CPU doing ~60% of the
// points, while a simple GPU-friendly flame rendered 5-10% slower in hybrid
// mode on long renders, with the CPU doing only ~20-24% - there the CPU adds
// little and its heat throttles the GPU. The CPU:GPU throughput ratio
// separates the two cases, which is what recommendBackend() uses.

#include <atomic>
#include <cstdint>

#include "../Flame.h"
#include "Renderer.h"

namespace apo {

enum class RenderBackend { Auto = 0, Cpu = 1, Gpu = 2, Hybrid = 3 };

struct BackendProbeResult {
    // False if the probe was aborted or failed; nothing below is meaningful.
    bool valid = false;
    // RenderDispatcher::wouldUseGpu() for this flame - false means the GPU
    // can't or shouldn't be used (disabled, no CUDA, plugin variation, or
    // too small), and gpuPointsPerSec is 0.
    bool gpuUsable = false;
    // Iteration throughput, points per second. cpuPointsPerSec is measured
    // with `cpuThreads` threads - hybrid's CPU side, one fewer than
    // threadCount since the calling thread drives CUDA.
    double cpuPointsPerSec = 0;
    int cpuThreads = 0;
    int threadCount = 0;
    double gpuPointsPerSec = 0;
};

// Below this CPU:GPU throughput ratio the GPU renders alone: the CPU would
// add under ~23% of the points, and on a laptop its heat costs the GPU more
// than that (measured at a ratio of ~0.26-0.32 on long renders). Above it,
// hybrid. From two flames on one laptop - a starting point, not a law.
constexpr double kHybridMinCpuToGpuRatio = 0.3;

// Runs the measurement: the real GPU render until its launch size settles
// (a few launches, ~0.3-2.5s, at most `gpuSeconds`; cancelled and its
// partial image discarded), then about `cpuSeconds` of CPU iteration. Blocks the calling thread; call it off the UI thread.
// `abort`, if given, is polled and makes the probe return early with
// valid = false. `threadCount` 0 = hardware_concurrency().
BackendProbeResult probeBackends(const Flame& flame, int threadCount, BucketPrecision precision, bool preferGpu,
                                 const std::atomic<bool>* abort = nullptr, double cpuSeconds = 0.3,
                                 double gpuSeconds = 3.0);

// Pure decision rule over a valid probe: Cpu when the GPU isn't usable,
// otherwise Hybrid when cpu/gpu >= kHybridMinCpuToGpuRatio, else Gpu.
RenderBackend recommendBackend(const BackendProbeResult& probe);

// Rough iteration time in seconds for `totalPoints` with `backend` (never
// Auto - resolve it first), from the probe's rates. Hybrid assumes the two
// rates add; CPU-only scales the probe's rate up to all threadCount threads.
// Long renders on thermally limited machines run slower than this, so treat
// it as "about". Returns 0 when the backend's rate is unknown.
double estimateRenderSeconds(const BackendProbeResult& probe, RenderBackend backend, std::uint64_t totalPoints);

// Points a render of `flame` will iterate (numBatches * sub-batch size),
// from the same plan arithmetic the renderers use. Cheap - no allocation.
std::uint64_t plannedPointCount(const Flame& flame);

} // namespace apo
