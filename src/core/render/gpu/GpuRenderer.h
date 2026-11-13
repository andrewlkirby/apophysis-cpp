#pragma once

#include <cstdint>

#include "../../Flame.h"
#include "../Renderer.h"

namespace apo::gpu {

// The CUDA render backend's public entry point - mirrors
// apo::Renderer::render()'s signature/return type (RenderedImage,
// RenderProgress, RenderTimings - all from Renderer.h, reused unchanged, see
// docs/GPU_RENDERING_PLAN.md) so RenderDispatcher can switch between the two
// backends with no shape difference visible to callers.
//
// Unlike Renderer::render(), this does NOT decide whether the GPU is an
// appropriate backend for `flame` - that's RenderDispatcher's job
// (isCudaAvailable() + isFlameGpuEligible()). Calling this on an ineligible
// flame or without a CUDA device present returns an empty RenderedImage{}.
// How the chaos kernel assigns xforms to GPU threads.
//   PerThread:   every thread draws its own xform each step (chaosKernel).
//   WarpUniform: all 32 threads of a warp apply the same xform each step and
//                points are shuffled between warps (chaosKernelWarpUniform);
//                avoids warp divergence when xforms run different variation
//                lists. Not valid with xaos - requesting it there falls back
//                to PerThread.
//   Auto:        WarpUniform when the flame has no xaos and its selectable
//                xforms don't all run the same variation list, else
//                PerThread.
enum class XformScheduling { Auto, PerThread, WarpUniform };

class GpuRenderer {
public:
    // What render() would actually use for `flame` given `requested` -
    // never Auto. For tests and benchmarks.
    static XformScheduling resolveScheduling(const Flame& flame, XformScheduling requested = XformScheduling::Auto);

    // `precision`: same BucketPrecision the CPU renderer takes (Renderer.h)
    // and the same tradeoff - Double (default) for every existing caller's
    // unchanged behavior, Float as an explicit opt-in for the low-density,
    // latency-sensitive interactive-preview path. See DeviceTypes.h's
    // DeviceBucketF comment for what actually changes on the GPU side.
    //
    // `cpuAssistThreads` > 0 turns on hybrid rendering
    // (docs/HYBRID_CPU_GPU_RENDERING_PLAN.md): that many CPU worker threads
    // iterate sub-batches alongside the GPU, claiming work from the same
    // counter, and stream their points to the GPU, which adds them into its
    // histogram on a second CUDA stream while chaos launches run. The calling
    // thread keeps driving CUDA, so callers typically pass
    // hardware_concurrency() - 1. 0 (default) is the unchanged GPU-only path.
    // Output is not bit-reproducible run to run in hybrid mode.
    static RenderedImage render(const Flame& flame, std::uint64_t seed, RenderProgress* progress = nullptr,
                                 RenderTimings* timings = nullptr, BucketPrecision precision = BucketPrecision::Double,
                                 int cpuAssistThreads = 0, XformScheduling scheduling = XformScheduling::Auto);
};

} // namespace apo::gpu
