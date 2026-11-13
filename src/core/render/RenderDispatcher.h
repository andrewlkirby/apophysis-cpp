#pragma once

#include <cstdint>
#include <string>

#include "../Flame.h"
#include "Renderer.h"

namespace apo {

// Picks between the CPU renderer (Renderer, always available - see
// Renderer.h) and the optional CUDA backend (gpu::GpuRenderer, see
// docs/GPU_RENDERING_PLAN.md), presenting one call surface so UI code
// doesn't need its own #ifdef APO_HAVE_CUDA branches. Compiled
// unconditionally, regardless of APO_ENABLE_CUDA - when this project wasn't
// built with CUDA support at all, every method here behaves exactly as if
// `preferGpu` were always false (GpuAvailability::isCudaAvailable() reports
// unavailable in that build, so wouldUseGpu()/render() both fall straight
// through to the CPU path with zero overhead).
class RenderDispatcher {
public:
    // Renders with fewer chaos-game sub-batches than this stay on the CPU
    // even when the GPU is otherwise usable (RENDER_OPTIMIZATION_PLAN.md
    // step 3). The GPU backend runs one sub-batch (kSubBatchSize = 10,000
    // sequential iterations) per device thread, so a small render launches
    // too few threads to fill the device and its wall time is bounded below
    // by one thread's 10,000-iteration latency (~0.1-0.25s on the dev
    // machine) - measured slower than the 16-thread CPU path for every
    // apo_bench fixture below ~1,000 sub-batches (e.g. 800x600 density 5:
    // CPU 0.05-0.08s vs GPU 0.10-0.28s), roughly even at ~1,000, and a
    // clear GPU win from ~2,000 up. Live-drag previews (~100-300
    // sub-batches) are exactly the case this keeps on the CPU.
    static constexpr int kMinGpuBatches = 1024;

    // Same shape as Renderer::render() plus `preferGpu` (default true - "use
    // the GPU when it's actually usable for this exact flame", matching
    // threadCount=0's "use a sensible default" convention). Falls back to
    // Renderer::render() with the same arguments when: preferGpu is false,
    // no CUDA device is available, the flame isn't GPU-eligible (uses a
    // variation with no GPU implementation, including any of the 47 legacy
    // C plugins - see gpu::isFlameGpuEligible), or the GPU render itself
    // fails for any reason (treated as a recoverable error, not surfaced to
    // the caller - matches Renderer::render()'s own "never throws for a
    // normal render" contract as closely as a fallback can).
    //
    // `usedGpu`, if non-null, is set to whether this specific call's
    // returned image actually came from the GPU backend - not just whether
    // GPU was attempted (wouldUseGpu() below predicts that cheaply, without
    // rendering, but can't know about a GPU failure the render() call
    // itself falls back from) - so a UI status indicator reflects what
    // really happened, not just what was expected to.
    //
    // `cpuAssist` (default false): when the GPU path is taken, also run
    // (threadCount - 1) CPU worker threads alongside it (threadCount 0 =
    // hardware_concurrency()), streaming their points to the GPU - see
    // gpu::GpuRenderer::render's cpuAssistThreads and
    // docs/HYBRID_CPU_GPU_RENDERING_PLAN.md. Ignored when the render goes
    // to the CPU anyway. Not bit-reproducible run to run.
    // `cpuAssistThreads` >= 0 sets hybrid's CPU worker count directly
    // instead of deriving it from threadCount (cpuAssistThreadCount). The
    // two differ when a caller has reduced threadCount to bound the CPU
    // renderer's per-thread histogram memory: hybrid's CPU workers keep no
    // histograms (they stream points to the GPU), so that limit doesn't
    // apply to them, while threadCount must still bound the CPU fallback
    // render if the GPU fails.
    static RenderedImage render(const Flame& flame, std::uint64_t seed, int threadCount = 0,
                                 RenderProgress* progress = nullptr, RenderTimings* timings = nullptr,
                                 BucketPrecision precision = BucketPrecision::Double, bool preferGpu = true,
                                 bool* usedGpu = nullptr, bool cpuAssist = false,
                                 int cpuAssistThreads = -1);

    // True iff render() with these arguments would run hybrid CPU+GPU:
    // wouldUseGpu() plus cpuAssist plus at least one CPU thread to spare.
    static bool wouldUseCpuAssist(const Flame& flame, bool preferGpu, bool cpuAssist, int threadCount = 0);

    // CPU worker threads a hybrid render runs for this threadCount
    // (threadCount 0 = hardware_concurrency()): one fewer, leaving a core
    // for the thread driving the GPU. 0 when there's none to spare.
    static int cpuAssistThreadCount(int threadCount);

    // True iff a render() call with these same `flame`/`preferGpu` would
    // actually use the GPU path - cheap, does no rendering. Meant for a UI
    // status indicator ("Rendering on GPU" / "CPU (plugin variation in
    // use)") - see docs/GPU_RENDERING_PLAN.md's UI integration section.
    static bool wouldUseGpu(const Flame& flame, bool preferGpu = true);

    // "" when wouldUseGpu(flame, preferGpu) is true; otherwise a short,
    // user-facing reason it isn't (GPU disabled in preferences, no CUDA
    // device, flame uses a CPU-only/legacy-plugin variation, render too
    // small to benefit - see kMinGpuBatches - or, in a non-CUDA build, CUDA
    // support not compiled in). Cheap, does no rendering; the
    // companion half of wouldUseGpu() for a UI backend indicator that
    // explains itself rather than just saying "CPU".
    static std::string cpuFallbackReason(const Flame& flame, bool preferGpu = true);

    // Estimated peak CUDA device-memory GpuRenderer::render() would use for
    // this exact flame/precision - see gpu::estimatePeakMemoryBytes's own
    // doc comment (DeviceFlame.h) for what's counted. 0 if no CUDA device is
    // available or the flame isn't GPU-eligible (callers should treat 0 as
    // "no estimate", not "zero bytes" - same convention as the underlying
    // gpu:: call). Deliberately NOT gated on `preferGpu`/AppSettings'
    // use-GPU toggle, unlike wouldUseGpu() - this answers "what would the
    // GPU path cost if used", independent of whether the user currently has
    // it enabled, so a UI can show it even while the preference is off.
    static std::uint64_t estimateGpuPeakMemoryBytes(const Flame& flame,
                                                      BucketPrecision precision = BucketPrecision::Double);
};

} // namespace apo
