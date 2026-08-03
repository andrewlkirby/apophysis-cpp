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
class GpuRenderer {
public:
    // `precision`: same BucketPrecision the CPU renderer takes (Renderer.h)
    // and the same tradeoff - Double (default) for every existing caller's
    // unchanged behavior, Float as an explicit opt-in for the low-density,
    // latency-sensitive interactive-preview path. See DeviceTypes.h's
    // DeviceBucketF comment for what actually changes on the GPU side.
    static RenderedImage render(const Flame& flame, std::uint64_t seed, RenderProgress* progress = nullptr,
                                 RenderTimings* timings = nullptr, BucketPrecision precision = BucketPrecision::Double);
};

} // namespace apo::gpu
