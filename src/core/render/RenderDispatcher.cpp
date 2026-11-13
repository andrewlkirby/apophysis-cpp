#include "RenderDispatcher.h"

#include <algorithm>
#include <thread>

#include "RenderPlan.h"
#include "gpu/DeviceFlame.h"
#include "gpu/GpuAvailability.h"

#ifdef APO_HAVE_CUDA
#include "gpu/GpuRenderer.h"
#endif

namespace apo {

namespace {

#ifdef APO_HAVE_CUDA
// Same sub-batch count Renderer::render()/GpuRenderer::render() will
// compute for this flame - reuses buildFilter/buildBuffersAndCamera in the
// same order render() calls them (same approach as
// Renderer::estimatePeakMemoryBytes), so it can't drift from the real
// formula. Pure arithmetic, no allocation of buckets.
bool isTooSmallForGpu(const Flame& flame) {
    detail::RenderPlan plan;
    detail::buildFilter(flame, plan);
    detail::buildBuffersAndCamera(flame, plan);
    return plan.numBatches < RenderDispatcher::kMinGpuBatches;
}
#endif

} // namespace

bool RenderDispatcher::wouldUseGpu(const Flame& flame, bool preferGpu) {
#ifdef APO_HAVE_CUDA
    return preferGpu && gpu::isCudaAvailable() && gpu::isFlameGpuEligible(flame) && !isTooSmallForGpu(flame);
#else
    (void)flame;
    (void)preferGpu;
    return false;
#endif
}

std::string RenderDispatcher::cpuFallbackReason(const Flame& flame, bool preferGpu) {
    if (!preferGpu) return "GPU rendering disabled in Options";
#ifdef APO_HAVE_CUDA
    if (!gpu::isCudaAvailable()) return gpu::cudaUnavailableReason();
    if (!gpu::isFlameGpuEligible(flame)) return "flame uses a CPU-only (legacy plugin) variation";
    if (isTooSmallForGpu(flame)) return "render too small to benefit from GPU";
    return {};
#else
    (void)flame;
    return gpu::cudaUnavailableReason();
#endif
}

int RenderDispatcher::cpuAssistThreadCount(int threadCount) {
    const int resolved =
        threadCount > 0 ? threadCount : static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    return std::max(0, resolved - 1);
}

bool RenderDispatcher::wouldUseCpuAssist(const Flame& flame, bool preferGpu, bool cpuAssist, int threadCount) {
    return cpuAssist && cpuAssistThreadCount(threadCount) > 0 && wouldUseGpu(flame, preferGpu);
}

std::uint64_t RenderDispatcher::estimateGpuPeakMemoryBytes(const Flame& flame, BucketPrecision precision) {
    if (!gpu::isCudaAvailable() || !gpu::isFlameGpuEligible(flame)) return 0;
    return gpu::estimatePeakMemoryBytes(flame, precision);
}

RenderedImage RenderDispatcher::render(const Flame& flame, std::uint64_t seed, int threadCount,
                                        RenderProgress* progress, RenderTimings* timings, BucketPrecision precision,
                                        bool preferGpu, bool* usedGpu, bool cpuAssist, int cpuAssistThreads) {
    if (usedGpu) *usedGpu = false;
#ifdef APO_HAVE_CUDA
    if (wouldUseGpu(flame, preferGpu)) {
        const int assistThreads =
            !cpuAssist ? 0 : cpuAssistThreads >= 0 ? cpuAssistThreads : cpuAssistThreadCount(threadCount);
        RenderedImage image =
            gpu::GpuRenderer::render(flame, seed, progress, timings, precision, assistThreads);
        // wouldUseGpu() already confirmed the flame is non-degenerate
        // (positive width/height, at least one xform), so an empty result
        // here means an actual CUDA failure (allocation, kernel launch,
        // etc) rather than the legitimate "degenerate flame" empty result
        // both backends can return - fall back to the CPU renderer rather
        // than surfacing a GPU-specific error to the caller.
        if (!image.pixels.empty()) {
            if (usedGpu) *usedGpu = true;
            return image;
        }
    }
#else
    (void)preferGpu;
    (void)cpuAssist;
    (void)cpuAssistThreads;
#endif
    return Renderer::render(flame, seed, threadCount, progress, timings, precision);
}

} // namespace apo
