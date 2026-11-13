#include "BackendProbe.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "CpuPointProducer.h"
#include "PointStream.h"
#include "RenderDispatcher.h"
#include "RenderPlan.h"

#ifdef APO_HAVE_CUDA
#include "gpu/GpuRenderer.h"
#endif

namespace apo {

namespace {

using clock_t = std::chrono::steady_clock;

double secondsSince(clock_t::time_point t0) { return std::chrono::duration<double>(clock_t::now() - t0).count(); }

bool aborted(const std::atomic<bool>* abort) { return abort && abort->load(std::memory_order_relaxed); }

int plannedBatches(const Flame& flame) {
    detail::RenderPlan plan;
    detail::buildFilter(flame, plan);
    detail::buildBuffersAndCamera(flame, plan);
    return plan.numBatches;
}

// Hybrid's CPU side, as hybrid runs it: CpuPointProducer workers iterating
// real sub-batches and streaming points, here into chunks that are simply
// recycled (no histogram, so nothing proportional to the image size is
// allocated). Stops the workers via RenderProgress cancel after `seconds`;
// each finishes the sub-batch it is on, so the overshoot is one sub-batch.
double measureCpu(const Flame& flame, int threads, double seconds, const std::atomic<bool>* abort) {
    constexpr int kChunkCapacity = 16 * detail::kSubBatchSize;
    const int chunkCount = threads + 2;
    std::vector<detail::StreamedPoint> storage(static_cast<size_t>(chunkCount) * kChunkCapacity);
    std::vector<detail::PointStream::Chunk> chunks(static_cast<size_t>(chunkCount));
    for (int i = 0; i < chunkCount; ++i) {
        chunks[static_cast<size_t>(i)].data = storage.data() + static_cast<size_t>(i) * kChunkCapacity;
        chunks[static_cast<size_t>(i)].capacity = kChunkCapacity;
    }
    detail::PointStream stream(std::move(chunks));
    std::atomic<int> nextBatch{0};
    RenderProgress control;

    const auto t0 = clock_t::now();
    detail::CpuPointProducer producer(flame, /*seed=*/0x5eed, threads, nextBatch, stream, &control);
    while (!producer.finished()) {
        if (detail::PointStream::Chunk* c = stream.tryPopFull()) {
            stream.release(c);
            continue;
        }
        if (secondsSince(t0) >= seconds || aborted(abort)) control.cancelRequested.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const RenderStats stats = producer.join();
    const double elapsed = secondsSince(t0);
    if (aborted(abort) || elapsed <= 0) return 0;
    return static_cast<double>(stats.pointsGenerated) / elapsed;
}

#ifdef APO_HAVE_CUDA
// The real GPU render of `flame` (same size and density, so histogram
// cache behavior and launch sizing are the real ones), cancelled once the
// renderer's adaptive launch size has settled. The first launch is a fixed
// guess (4096 sub-batches) whose throughput can be far from the settled
// rate in either direction - for an expensive flame, launches shrink to
// stay under the per-launch time target and the GPU runs much less
// occupied - so the rate is the last full-size launch's
// (RenderTimings::gpuLastLaunchPointsPerSec) after kSettleLaunches
// launches, or whatever ran by `maxSeconds`. Launch completions are seen as
// RenderProgress::pointsDone ticks.
double measureGpu(const Flame& flame, BucketPrecision precision, double maxSeconds, const std::atomic<bool>* abort) {
    constexpr int kSettleLaunches = 6;
    RenderProgress control;
    std::atomic<bool> done{false};
    std::thread timer([&] {
        const auto t0 = clock_t::now();
        std::uint64_t lastDone = 0;
        int launches = 0;
        while (!done.load() && secondsSince(t0) < maxSeconds && !aborted(abort)) {
            const std::uint64_t d = control.pointsDone.load(std::memory_order_relaxed);
            if (d != lastDone) {
                lastDone = d;
                if (++launches >= kSettleLaunches) break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        control.cancelRequested.store(true);
    });
    RenderTimings timings;
    const RenderedImage img = gpu::GpuRenderer::render(flame, /*seed=*/0x5eed, &control, &timings, precision);
    done.store(true);
    timer.join();
    if (img.pixels.empty() || aborted(abort)) return 0;
    if (timings.gpuLastLaunchPointsPerSec > 0) return timings.gpuLastLaunchPointsPerSec;
    // Finished before any full-size launch completed (a small render).
    return timings.iterateSeconds > 0 ? static_cast<double>(img.stats.pointsGenerated) / timings.iterateSeconds : 0;
}
#endif

} // namespace

std::uint64_t plannedPointCount(const Flame& flame) {
    return static_cast<std::uint64_t>(plannedBatches(flame)) * detail::kSubBatchSize;
}

BackendProbeResult probeBackends(const Flame& flame, int threadCount, BucketPrecision precision, bool preferGpu,
                                 const std::atomic<bool>* abort, double cpuSeconds, double gpuSeconds) {
    BackendProbeResult r;
    r.threadCount =
        threadCount > 0 ? threadCount : static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    r.gpuUsable = RenderDispatcher::wouldUseGpu(flame, preferGpu);
    // Measured with hybrid's thread count when the GPU is in play (that's
    // the CPU side being decided on); with all threads otherwise.
    r.cpuThreads = r.gpuUsable ? std::max(1, RenderDispatcher::cpuAssistThreadCount(r.threadCount)) : r.threadCount;

#ifdef APO_HAVE_CUDA
    // GPU first: it also pays any one-off CUDA context start-up, which then
    // doesn't sit on the CPU measurement's clock.
    if (r.gpuUsable) {
        r.gpuPointsPerSec = measureGpu(flame, precision, gpuSeconds, abort);
        if (r.gpuPointsPerSec <= 0) return r;
    }
#else
    (void)precision;
#endif
    if (aborted(abort)) return r;
    r.cpuPointsPerSec = measureCpu(flame, r.cpuThreads, cpuSeconds, abort);
    r.valid = r.cpuPointsPerSec > 0 && !aborted(abort);
    return r;
}

RenderBackend recommendBackend(const BackendProbeResult& probe) {
    if (!probe.gpuUsable || probe.gpuPointsPerSec <= 0) return RenderBackend::Cpu;
    return probe.cpuPointsPerSec >= kHybridMinCpuToGpuRatio * probe.gpuPointsPerSec ? RenderBackend::Hybrid
                                                                                    : RenderBackend::Gpu;
}

double estimateRenderSeconds(const BackendProbeResult& probe, RenderBackend backend, std::uint64_t totalPoints) {
    // CPU-only runs every thread, the probe measured cpuThreads of them.
    const double cpuAll = probe.cpuThreads > 0 ? probe.cpuPointsPerSec * probe.threadCount / probe.cpuThreads : 0;
    double rate = 0;
    switch (backend) {
        case RenderBackend::Cpu: rate = cpuAll; break;
        case RenderBackend::Gpu: rate = probe.gpuPointsPerSec; break;
        case RenderBackend::Hybrid: rate = probe.gpuPointsPerSec > 0 ? probe.gpuPointsPerSec + probe.cpuPointsPerSec : 0; break;
        case RenderBackend::Auto: return estimateRenderSeconds(probe, recommendBackend(probe), totalPoints);
    }
    return rate > 0 ? static_cast<double>(totalPoints) / rate : 0;
}

} // namespace apo
