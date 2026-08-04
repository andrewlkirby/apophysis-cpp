#include "RenderDispatcher.h"

#include "gpu/DeviceFlame.h"
#include "gpu/GpuAvailability.h"

#ifdef APO_HAVE_CUDA
#include "gpu/GpuRenderer.h"
#endif

namespace apo {

bool RenderDispatcher::wouldUseGpu(const Flame& flame, bool preferGpu) {
#ifdef APO_HAVE_CUDA
    return preferGpu && gpu::isCudaAvailable() && gpu::isFlameGpuEligible(flame);
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
    return {};
#else
    (void)flame;
    return gpu::cudaUnavailableReason();
#endif
}

std::uint64_t RenderDispatcher::estimateGpuPeakMemoryBytes(const Flame& flame, BucketPrecision precision) {
    if (!gpu::isCudaAvailable() || !gpu::isFlameGpuEligible(flame)) return 0;
    return gpu::estimatePeakMemoryBytes(flame, precision);
}

RenderedImage RenderDispatcher::render(const Flame& flame, std::uint64_t seed, int threadCount,
                                        RenderProgress* progress, RenderTimings* timings, BucketPrecision precision,
                                        bool preferGpu, bool* usedGpu) {
    if (usedGpu) *usedGpu = false;
#ifdef APO_HAVE_CUDA
    if (wouldUseGpu(flame, preferGpu)) {
        RenderedImage image = gpu::GpuRenderer::render(flame, seed, progress, timings, precision);
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
#endif
    return Renderer::render(flame, seed, threadCount, progress, timings, precision);
}

} // namespace apo
