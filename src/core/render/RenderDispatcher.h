#pragma once

#include <cstdint>

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
    static RenderedImage render(const Flame& flame, std::uint64_t seed, int threadCount = 0,
                                 RenderProgress* progress = nullptr, RenderTimings* timings = nullptr,
                                 BucketPrecision precision = BucketPrecision::Double, bool preferGpu = true,
                                 bool* usedGpu = nullptr);

    // True iff a render() call with these same `flame`/`preferGpu` would
    // actually use the GPU path - cheap, does no rendering. Meant for a UI
    // status indicator ("Rendering on GPU" / "CPU (plugin variation in
    // use)") - see docs/GPU_RENDERING_PLAN.md's UI integration section.
    static bool wouldUseGpu(const Flame& flame, bool preferGpu = true);
};

} // namespace apo
