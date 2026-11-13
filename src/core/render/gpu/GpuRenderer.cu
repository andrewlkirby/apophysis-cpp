#include "GpuRenderer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include <cuda_runtime.h>

#include "../CpuPointProducer.h"
#include "../PointStream.h"
#include "../RenderPlan.h"
#include "DeviceFlame.h"
#include "DeviceTypes.h"
#include "chaos_kernel.cuh"
#include "tonemap_kernel.cuh"

namespace apo::gpu {

namespace {

// Minimal RAII device-memory buffer - cudaMalloc's max(1,count) elements
// (never a literal zero-byte allocation, which some driver versions treat
// as an error) so every DeviceBuffer in render() below can be constructed
// uniformly regardless of whether that particular array is actually used
// for this flame (e.g. dVarParams when no active variation has named
// parameters). `count()` still reports the logical (possibly zero) size,
// and uploadFrom/downloadTo no-op at count 0 - callers never see the
// padding.
template <class T>
class DeviceBuffer {
public:
    DeviceBuffer() = default;
    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
    DeviceBuffer(DeviceBuffer&& other) noexcept { *this = std::move(other); }
    DeviceBuffer& operator=(DeviceBuffer&& other) noexcept {
        if (this != &other) {
            freeIfNeeded();
            ptr_ = other.ptr_;
            count_ = other.count_;
            other.ptr_ = nullptr;
            other.count_ = 0;
        }
        return *this;
    }
    ~DeviceBuffer() { freeIfNeeded(); }

    bool allocate(size_t count) {
        freeIfNeeded();
        count_ = count;
        const size_t allocCount = count == 0 ? 1 : count;
        return cudaMalloc(reinterpret_cast<void**>(&ptr_), allocCount * sizeof(T)) == cudaSuccess;
    }
    bool uploadFrom(const T* src, size_t n) {
        if (n == 0) return true;
        return cudaMemcpy(ptr_, src, n * sizeof(T), cudaMemcpyHostToDevice) == cudaSuccess;
    }
    bool uploadFrom(const std::vector<T>& v) { return uploadFrom(v.data(), v.size()); }
    bool downloadTo(T* dst, size_t n) const {
        if (n == 0) return true;
        return cudaMemcpy(dst, ptr_, n * sizeof(T), cudaMemcpyDeviceToHost) == cudaSuccess;
    }
    bool zero() {
        const size_t allocCount = count_ == 0 ? 1 : count_;
        return cudaMemset(ptr_, 0, allocCount * sizeof(T)) == cudaSuccess;
    }
    T* get() const { return ptr_; }
    size_t count() const { return count_; }

private:
    void freeIfNeeded() {
        if (ptr_) cudaFree(ptr_);
        ptr_ = nullptr;
        count_ = 0;
    }
    T* ptr_ = nullptr;
    size_t count_ = 0;
};

// Launch sizing (see DeviceChaosWave in chaos_kernel.cuh for the wave/step
// model). A render runs waves of up to `slots` sub-batches - one per
// thread, slots sized to what the GPU can keep resident at once - and each
// launch advances the whole wave by some number of steps. That step count
// is what adapts toward kTargetLaunchSeconds, not the thread count: with
// every thread resident, launch time is proportional to steps, so the
// controller's linear model holds, and the GPU stays fully occupied however
// expensive the flame. (Before, each thread ran a whole 10,016-step
// sub-batch per launch, so for an expensive flame the time budget forced
// the thread count down to ~1000 and most of the GPU sat idle.)
//
// kInitialStepsPerLaunch is the conservative first guess every render
// starts from (a few ms for a cheap flame, well under the budget for an
// expensive one); kMinStepsPerLaunch is a floor a render never adapts
// below even if a launch measures ~0s.
constexpr int kInitialStepsPerLaunch = 32;
constexpr int kMinStepsPerLaunch = 1;
constexpr int kThreadsPerBlock = kChaosThreadsPerBlock;

// Target wall-clock time for a single chaos-kernel launch. Deliberately far
// below Windows WDDM's Timeout Detection and Recovery watchdog (2 seconds by
// default - a GPU that doesn't yield to the display driver within that
// window gets its driver reset by the OS, killing the render and every other
// GPU-using app with it, not just this one) - this is a hard safety ceiling,
// not a tunable preference, so it's not exposed anywhere a user could push
// it toward that edge. 0.35s keeps real launches comfortably under TDR even
// after kMaxGrowthFactor's headroom below overshoots on a single step, while
// still being short enough that cancel/pause and the progress bar stay
// responsive.
constexpr double kTargetLaunchSeconds = 0.35;
// Per-launch clamp on how fast adaptStepsPerLaunch() can grow the next
// launch - without this, a single unrepresentative fast measurement (e.g. a
// lucky GPU boost-clock window) could jump straight to a launch several
// times larger than intended and land close to the TDR ceiling on the very
// next try. Shrinking is not similarly capped: overshooting *down* just
// costs a bit of throughput on the next launch, while overshooting *up*
// risks the TDR reset above.
constexpr double kMaxGrowthFactor = 2.0;
constexpr double kFastLaunchSeconds = 0.05;
constexpr double kFastGrowthFactor = 8.0;

void waitWhilePaused(RenderProgress* progress) {
    if (!progress) return;
    while (progress->pauseRequested.load(std::memory_order_relaxed) &&
           !progress->cancelRequested.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

// Self-calibrating launch-size controller. `lastSeconds` is how long the
// just-finished launch of `lastSteps` steps took; returns the step count
// for the next launch, aimed at kTargetLaunchSeconds. Proportional: per-step
// cost differs by orders of magnitude between flames, and a ratio reaches
// the right size in a few launches from either extreme. Pure function, no
// device access.
int adaptStepsPerLaunch(int lastSteps, double lastSeconds) {
    if (lastSeconds <= 0.0) return std::max(lastSteps, kMinStepsPerLaunch);
    // Launches far below the budget may grow faster: even 8x a sub-50ms
    // launch stays near 0.4s, well inside the watchdog, and it cuts the
    // ramp-up from the conservative first guess to a few launches.
    const double cap = lastSeconds < kFastLaunchSeconds ? kFastGrowthFactor : kMaxGrowthFactor;
    const double ratio = std::min(kTargetLaunchSeconds / lastSeconds, cap);
    const double next = static_cast<double>(lastSteps) * ratio;
    return std::clamp(static_cast<int>(next), kMinStepsPerLaunch, kDeviceSubBatchSteps);
}

// How many sub-batches the GPU can work on at once: as many threads as it
// keeps resident for `kernel`, rounded to whole blocks. More would only
// queue; fewer leaves SMs idle.
template <class Kernel>
int residentSlots(Kernel kernel) {
    int device = 0, sms = 0, blocksPerSm = 0;
    if (cudaGetDevice(&device) != cudaSuccess ||
        cudaDeviceGetAttribute(&sms, cudaDevAttrMultiProcessorCount, device) != cudaSuccess ||
        cudaOccupancyMaxActiveBlocksPerMultiprocessor(&blocksPerSm, kernel, kThreadsPerBlock, 0) != cudaSuccess) {
        return 16 * kThreadsPerBlock;
    }
    return std::max(1, sms * std::max(1, blocksPerSm)) * kThreadsPerBlock;
}

// ---------------------------------------------------------------------
// Hybrid CPU+GPU rendering (docs/HYBRID_CPU_GPU_RENDERING_PLAN.md)
// ---------------------------------------------------------------------

static_assert(sizeof(DeviceStreamedPoint) == sizeof(detail::StreamedPoint), "point wire format mismatch");

// Sub-batches per host chunk: 16 * 10,000 points * 8 bytes = 1.28 MB.
constexpr int kChunkSubBatches = 16;
constexpr int kChunkCapacity = kChunkSubBatches * detail::kSubBatchSize;
// Device staging buffers - how many chunk uploads/scatters can be in flight
// on the side stream at once.
constexpr int kStagingSlots = 4;

// Everything the CPU-assist half of a render owns: pinned host chunk pool,
// device staging slots, side stream, and the CPU producer. Destruction order
// matters and is explicit in the destructor: stop the producers, wait for
// any in-flight async copies, then free memory.
class HybridAssist {
public:
    HybridAssist() = default;
    HybridAssist(const HybridAssist&) = delete;
    HybridAssist& operator=(const HybridAssist&) = delete;

    ~HybridAssist() {
        if (stream_) stream_->abort();
        producer_.reset(); // joins worker threads
        if (side_) cudaStreamSynchronize(side_);
        for (Slot& s : slots_) {
            if (s.done) cudaEventDestroy(s.done);
        }
        if (side_) cudaStreamDestroy(side_);
        if (pinned_) cudaFreeHost(pinned_);
    }

    // Allocates everything and starts the CPU workers. False on any CUDA
    // failure or plan mismatch.
    bool start(const Flame& flame, std::uint64_t seed, int threads, const DeviceFlameHost& host,
               RenderProgress* progress) {
        const int numChunks = 2 * threads + kStagingSlots;
        const size_t poolPoints = static_cast<size_t>(numChunks) * kChunkCapacity;
        if (cudaHostAlloc(reinterpret_cast<void**>(&pinned_), poolPoints * sizeof(detail::StreamedPoint),
                          cudaHostAllocDefault) != cudaSuccess) {
            pinned_ = nullptr;
            return false;
        }
        if (cudaStreamCreateWithFlags(&side_, cudaStreamNonBlocking) != cudaSuccess) {
            side_ = nullptr;
            return false;
        }
        for (Slot& s : slots_) {
            if (!s.dev.allocate(kChunkCapacity)) return false;
            if (cudaEventCreateWithFlags(&s.done, cudaEventDisableTiming) != cudaSuccess) {
                s.done = nullptr;
                return false;
            }
        }

        std::vector<detail::PointStream::Chunk> chunks(static_cast<size_t>(numChunks));
        for (int i = 0; i < numChunks; ++i) {
            chunks[static_cast<size_t>(i)].data = pinned_ + static_cast<size_t>(i) * kChunkCapacity;
            chunks[static_cast<size_t>(i)].capacity = kChunkCapacity;
        }
        stream_ = std::make_unique<detail::PointStream>(std::move(chunks));
        producer_ = std::make_unique<detail::CpuPointProducer>(flame, seed, threads, nextBatch_, *stream_, progress);
        // Both sides build their geometry from the same RenderPlan code, so
        // this can't differ - checked anyway, since a mismatch would write
        // CPU points to the wrong buckets.
        return producer_->bucketWidth() == host.bucketWidth && producer_->bucketHeight() == host.bucketHeight &&
               producer_->numBatches() == host.numBatches;
    }

    std::atomic<int>& nextBatch() { return nextBatch_; }

    // Retires finished uploads and starts new ones for any waiting chunks.
    // Returns false on a CUDA error. `didWork` reports whether anything
    // happened, so callers can yield when idle.
    bool pump(const DeviceKernelParams& kp, bool& didWork) {
        didWork = false;
        for (Slot& s : slots_) {
            if (!s.chunk) continue;
            const cudaError_t q = cudaEventQuery(s.done);
            if (q == cudaErrorNotReady) continue;
            if (q != cudaSuccess) return false;
            stream_->release(s.chunk);
            s.chunk = nullptr;
            didWork = true;
        }
        for (Slot& s : slots_) {
            if (s.chunk) continue;
            detail::PointStream::Chunk* c = stream_->tryPopFull();
            if (!c) break;
            const int n = c->count;
            if (cudaMemcpyAsync(s.dev.get(), c->data, static_cast<size_t>(n) * sizeof(DeviceStreamedPoint),
                                cudaMemcpyHostToDevice, side_) != cudaSuccess) {
                return false;
            }
            scatterKernel<<<(n + kThreadsPerBlock - 1) / kThreadsPerBlock, kThreadsPerBlock, 0, side_>>>(
                kp, s.dev.get(), n);
            if (cudaGetLastError() != cudaSuccess) return false;
            if (cudaEventRecord(s.done, side_) != cudaSuccess) return false;
            s.chunk = c;
            didWork = true;
        }
        return true;
    }

    // Waits for the CPU workers to finish while continuing to pump, then
    // drains every remaining chunk and syncs the side stream. Rethrows a
    // CPU worker exception (from CpuPointProducer::join()).
    bool finish(const DeviceKernelParams& kp, RenderStats& cpuStats) {
        bool didWork = false;
        while (!producer_->finished()) {
            if (!pump(kp, didWork)) return false;
            if (!didWork) std::this_thread::yield();
        }
        cpuStats = producer_->join();
        for (;;) {
            if (!pump(kp, didWork)) return false;
            bool busy = stream_->hasFull();
            for (const Slot& s : slots_) busy = busy || s.chunk != nullptr;
            if (!busy) break;
            if (!didWork) std::this_thread::yield();
        }
        return cudaStreamSynchronize(side_) == cudaSuccess;
    }

private:
    struct Slot {
        DeviceBuffer<DeviceStreamedPoint> dev;
        cudaEvent_t done = nullptr;
        detail::PointStream::Chunk* chunk = nullptr;
    };

    std::atomic<int> nextBatch_{0};
    detail::StreamedPoint* pinned_ = nullptr;
    cudaStream_t side_ = nullptr;
    Slot slots_[kStagingSlots];
    std::unique_ptr<detail::PointStream> stream_;
    std::unique_ptr<detail::CpuPointProducer> producer_;
};

bool sameOpKinds(const DeviceFlameHost& host, int a, int b) {
    const DeviceXform& xa = host.xforms[static_cast<size_t>(a)];
    const DeviceXform& xb = host.xforms[static_cast<size_t>(b)];
    if (xa.opsCount != xb.opsCount) return false;
    for (int i = 0; i < xa.opsCount; ++i) {
        if (host.ops[static_cast<size_t>(xa.opsOffset + i)].kind != host.ops[static_cast<size_t>(xb.opsOffset + i)].kind) {
            return false;
        }
    }
    return true;
}

XformScheduling resolveForHost(const DeviceFlameHost& host, XformScheduling requested) {
    if (requested == XformScheduling::PerThread || host.numXForms <= 0) return XformScheduling::PerThread;
    // The warp-uniform kernel draws from row 0 for every point, so every row
    // must match: any xaos (per-xform next-xform weights) rules it out.
    const int stride = host.propTableStride;
    const auto row0 = host.propTable.begin();
    for (int k = 1; k < host.numXForms; ++k) {
        if (!std::equal(row0, row0 + stride, row0 + static_cast<std::ptrdiff_t>(k) * stride)) {
            return XformScheduling::PerThread;
        }
    }
    if (requested == XformScheduling::WarpUniform) return XformScheduling::WarpUniform;
    // Auto: only worth it when threads picking different xforms would run
    // different code. Xforms sharing one variation list differ only in
    // their coefficients, which costs chaosKernel no divergence, and there
    // the shuffle's barrier per step would be pure overhead.
    std::vector<char> selectable(static_cast<size_t>(host.numXForms), 0);
    for (int i = 0; i < stride; ++i) selectable[static_cast<size_t>(host.propTable[static_cast<size_t>(i)])] = 1;
    int first = -1;
    for (int k = 0; k < host.numXForms; ++k) {
        if (!selectable[static_cast<size_t>(k)]) continue;
        if (first < 0) {
            first = k;
        } else if (!sameOpKinds(host, first, k)) {
            return XformScheduling::WarpUniform;
        }
    }
    return XformScheduling::PerThread;
}

} // namespace

XformScheduling GpuRenderer::resolveScheduling(const Flame& flame, XformScheduling requested) {
    DeviceFlameHost host;
    if (!buildDeviceFlame(flame, host)) return XformScheduling::PerThread;
    return resolveForHost(host, requested);
}

RenderedImage GpuRenderer::render(const Flame& flame, std::uint64_t seed, RenderProgress* progress,
                                   RenderTimings* timings, BucketPrecision precision, int cpuAssistThreads,
                                   XformScheduling scheduling) {
    const bool useFloatBuckets = (precision == BucketPrecision::Float);
    using clock = std::chrono::steady_clock;
    const auto secondsBetween = [](clock::time_point a, clock::time_point b) {
        return std::chrono::duration<double>(b - a).count();
    };
    const auto tStart = clock::now();

    DeviceFlameHost host;
    if (!buildDeviceFlame(flame, host)) return RenderedImage{};

    const long long bucketCount = static_cast<long long>(host.bucketWidth) * host.bucketHeight;
    if (bucketCount <= 0) return RenderedImage{};
    const bool warpUniform = resolveForHost(host, scheduling) == XformScheduling::WarpUniform;

    DeviceBuffer<DeviceXform> dXforms;
    DeviceBuffer<DeviceOp> dOps;
    DeviceBuffer<double> dVarParams;
    DeviceBuffer<int> dPropTable;
    DeviceBuffer<double> dColorMap;
    DeviceBuffer<double> dFilter;
    DeviceBuffer<double> dLogScale;
    DeviceBuffer<double> dCurveTable;
    DeviceBuffer<double> dDeKernels;
    DeviceBuffer<int> dDeOffsets;
    DeviceBuffer<DeviceBucket> dBucketsD;
    DeviceBuffer<DeviceBucketF> dBucketsF;
    DeviceBuffer<unsigned char> dPixels;
    DeviceBuffer<unsigned long long> dPointsGenerated;
    DeviceBuffer<unsigned long long> dPointsAccepted;

    bool ok = true;
    ok = ok && dXforms.allocate(host.xforms.size()) && dXforms.uploadFrom(host.xforms);
    ok = ok && dOps.allocate(host.ops.size()) && dOps.uploadFrom(host.ops);
    ok = ok && dVarParams.allocate(host.params.size()) && dVarParams.uploadFrom(host.params);
    ok = ok && dPropTable.allocate(host.propTable.size()) && dPropTable.uploadFrom(host.propTable);
    ok = ok && dColorMap.allocate(host.colorMap.size()) && dColorMap.uploadFrom(host.colorMap);
    ok = ok && dFilter.allocate(host.filter.size()) && dFilter.uploadFrom(host.filter);
    ok = ok && dLogScale.allocate(host.logScale.size()) &&
         dLogScale.uploadFrom(host.logScale.data(), host.logScale.size());
    ok = ok && dCurveTable.allocate(4 * 257) && dCurveTable.uploadFrom(&host.curveTable[0][0], 4 * 257);
    ok = ok && dDeKernels.allocate(host.deKernelsFlat.size()) && dDeKernels.uploadFrom(host.deKernelsFlat);
    ok = ok && dDeOffsets.allocate(host.deKernelOffsets.size()) && dDeOffsets.uploadFrom(host.deKernelOffsets);
    if (useFloatBuckets) {
        ok = ok && dBucketsF.allocate(static_cast<size_t>(bucketCount)) && dBucketsF.zero();
    } else {
        ok = ok && dBucketsD.allocate(static_cast<size_t>(bucketCount)) && dBucketsD.zero();
    }
    ok = ok && dPixels.allocate(static_cast<size_t>(host.width) * host.height * host.channels);
    ok = ok && dPointsGenerated.allocate(1) && dPointsGenerated.zero();
    ok = ok && dPointsAccepted.allocate(1) && dPointsAccepted.zero();
    if (!ok) return RenderedImage{};

    DeviceKernelParams kp;
    kp.numXForms = host.numXForms;
    kp.finalXformIndex = host.finalXformIndex;
    kp.propTableStride = host.propTableStride;
    kp.projectionKind = host.projectionKind;
    kp.cameraMatrix = host.cameraMatrix;
    kp.persp = host.persp;
    kp.zpos = host.zpos;
    kp.dofCoef = host.dofCoef;
    kp.camX0 = host.camX0;
    kp.camY0 = host.camY0;
    kp.camW = host.camW;
    kp.camH = host.camH;
    kp.bws = host.bws;
    kp.bhs = host.bhs;
    kp.bucketWidth = host.bucketWidth;
    kp.bucketHeight = host.bucketHeight;
    kp.xforms = dXforms.get();
    kp.ops = dOps.get();
    kp.varParams = dVarParams.get();
    kp.propTable = dPropTable.get();
    kp.colorMap = dColorMap.get();
    kp.useFloatBuckets = useFloatBuckets ? 1 : 0;
    kp.bucketsD = dBucketsD.get();
    kp.bucketsF = dBucketsF.get();

    if (progress) {
        progress->pointsDone.store(0, std::memory_order_relaxed);
        progress->pointsTarget.store(static_cast<std::uint64_t>(host.numBatches) * kDeviceSubBatchSize,
                                      std::memory_order_relaxed);
    }

    // Hybrid mode: started after progress is reset above, since the CPU
    // workers add to pointsDone as soon as they run.
    std::unique_ptr<HybridAssist> assist;
    if (cpuAssistThreads > 0) {
        assist = std::make_unique<HybridAssist>();
        if (!assist->start(flame, seed, cpuAssistThreads, host, progress)) return RenderedImage{};
    }
    // GPU-only renders claim sub-batches from a local counter; hybrid ones
    // from the counter shared with the CPU workers.
    std::atomic<int> localNextBatch{0};
    std::atomic<int>& nextBatch = assist ? assist->nextBatch() : localNextBatch;

    const auto tSetupEnd = clock::now();

    // Waves of up to `slots` sub-batches (see DeviceChaosWave), each
    // advanced launch by launch. The step count per launch ramps toward
    // kTargetLaunchSeconds via adaptStepsPerLaunch(), starting from the same
    // conservative kInitialStepsPerLaunch every render: re-measuring from
    // scratch is cheap (a few short launches) and self-corrects for
    // anything that changed since the last render (thermal state, another
    // app on the GPU, a different flame's per-point cost).
    const int slots = warpUniform ? residentSlots(chaosKernelWarpUniform) : residentSlots(chaosKernel);
    DeviceBuffer<DeviceChaosState> dStates;
    if (!dStates.allocate(static_cast<size_t>(slots))) return RenderedImage{};

    int stepsPerLaunch = kInitialStepsPerLaunch;
    int lastCount = 0;
    double lastLaunchPointsPerSec = 0;
    bool cancelled = false;
    while (!cancelled) {
        if (progress && progress->cancelRequested.load(std::memory_order_relaxed)) break;

        // Claim the next wave: always a full one (or whatever is left). In
        // hybrid mode the counter is shared, so it can already be past
        // numBatches (the CPU workers took the rest).
        //
        // A wave runs all kDeviceSubBatchSteps steps whatever its size, and
        // a partly filled wave takes nearly as long as a full one (per-step
        // time is set by the busiest SM, and a thinner wave mostly just
        // leaves SMs idle - measured: splitting 63,202 sub-batches into 4
        // equal waves of ~15,800 on 20,480 slots was 10-20% slower than 3
        // full waves plus a 1,922 remainder). So a render pays at most one
        // short final wave - under a second even for expensive flames, ~10%
        // of a sub-second render - and nothing is gained by shrinking waves,
        // including at the end of a hybrid render, where the CPU workers
        // simply finish what the GPU didn't claim.
        const int want = slots;
        const int batchOffset = nextBatch.fetch_add(want, std::memory_order_relaxed);
        if (batchOffset >= host.numBatches) break;

        DeviceChaosWave wave;
        wave.renderSeed = seed;
        wave.batchOffset = batchOffset;
        wave.count = std::min(want, host.numBatches - batchOffset);
        const int blocks = (wave.count + kThreadsPerBlock - 1) / kThreadsPerBlock;
        // A bigger wave than the last costs proportionally more per step
        // once it no longer fits the GPU at once; scale down to stay in the
        // time budget. (Smaller waves aren't assumed cheaper - per-step cost
        // is ~flat while the wave fits.)
        if (lastCount > 0 && wave.count > lastCount) {
            stepsPerLaunch = std::max(
                kMinStepsPerLaunch,
                static_cast<int>(static_cast<long long>(stepsPerLaunch) * lastCount / wave.count));
        }

        for (int step = 0; step < kDeviceSubBatchSteps;) {
            if (progress && progress->cancelRequested.load(std::memory_order_relaxed)) {
                cancelled = true;
                break;
            }
            waitWhilePaused(progress);
            if (progress && progress->cancelRequested.load(std::memory_order_relaxed)) {
                cancelled = true;
                break;
            }

            wave.stepBegin = step;
            wave.stepEnd = std::min(kDeviceSubBatchSteps, step + stepsPerLaunch);
            const auto tLaunchStart = clock::now();
            if (warpUniform) {
                chaosKernelWarpUniform<<<blocks, kThreadsPerBlock>>>(kp, wave, dStates.get(), dPointsGenerated.get(),
                                                                   dPointsAccepted.get());
            } else {
                chaosKernel<<<blocks, kThreadsPerBlock>>>(kp, wave, dStates.get(), dPointsGenerated.get(),
                                                            dPointsAccepted.get());
            }
            if (cudaGetLastError() != cudaSuccess) return RenderedImage{};
            if (assist) {
                // Keep feeding CPU points to the side stream while this launch
                // runs, instead of blocking in cudaDeviceSynchronize. Spins on
                // the core left free for this thread; a sleep here would add the
                // Windows timer granularity (1-15ms) to every launch.
                for (;;) {
                    const cudaError_t q = cudaStreamQuery(0);
                    if (q == cudaSuccess) break;
                    if (q != cudaErrorNotReady) return RenderedImage{};
                    bool didWork = false;
                    if (!assist->pump(kp, didWork)) return RenderedImage{};
                    if (!didWork) std::this_thread::yield();
                }
            } else if (cudaDeviceSynchronize() != cudaSuccess) {
                return RenderedImage{};
            }
            const double launchSeconds = secondsBetween(tLaunchStart, clock::now());
            const int stepsRun = wave.stepEnd - wave.stepBegin;
            // A launch that ran its full requested step count is the
            // steady-state rate a long render settles at
            // (RenderTimings::gpuLastLaunchPointsPerSec). Waves are balanced
            // (see above), so all but a hybrid render's tail waves are
            // close to full.
            if (stepsRun == stepsPerLaunch && launchSeconds > 0) {
                lastLaunchPointsPerSec = static_cast<double>(wave.count) * stepsRun / launchSeconds;
            }
            // Only launches that ran the requested step count recalibrate: a
            // wave's last launch can be cut short, and growing from its tiny
            // size would collapse the next wave's launches.
            if (stepsRun == stepsPerLaunch) stepsPerLaunch = adaptStepsPerLaunch(stepsRun, launchSeconds);
            lastCount = wave.count;

            if (progress) {
                // Plotted steps only (FUSE warm-up isn't part of pointsTarget),
                // so a completed render lands exactly on numBatches * 10,000.
                const int plotted =
                    std::max(0, wave.stepEnd - std::max(wave.stepBegin, kDeviceFuse + 1));
                progress->pointsDone.fetch_add(static_cast<std::uint64_t>(wave.count) * plotted,
                                                std::memory_order_relaxed);
            }
            step = wave.stepEnd;
        }
    }

    // Hybrid mode: wait for the CPU workers' last sub-batches and add every
    // remaining streamed point before tone-mapping.
    RenderStats cpuStats;
    if (assist && !assist->finish(kp, cpuStats)) return RenderedImage{};

    const auto tIterateEnd = clock::now();
    // No separate reduction pass on this backend - every thread atomicAdd's
    // straight into the shared device histogram during iteration (see
    // DeviceTypes.h's mapping comment), unlike the CPU renderer's
    // private-per-thread-bucket-then-reduce design. Hybrid mode's CPU points
    // are added during iteration too, by scatterKernel.
    const auto tReduceEnd = tIterateEnd;

    DeviceToneMapParams tp;
    tp.width = host.width;
    tp.height = host.height;
    tp.channels = host.channels;
    tp.oversample = host.oversample;
    tp.maxGutterWidth = host.maxGutterWidth;
    tp.bucketWidth = host.bucketWidth;
    tp.bucketHeight = host.bucketHeight;
    tp.filterSize = host.filterSize;
    tp.fastBucket = host.fastBucket ? 1 : 0;
    tp.k1 = host.k1;
    tp.k2 = host.k2;
    tp.whiteLevel = static_cast<double>(flame.whiteLevel);
    tp.gamma = host.gamma;
    tp.gammaThreshold = host.gammaThreshold;
    tp.vibrancy = host.vibrancy;
    for (int i = 0; i < 4; ++i) tp.background[i] = host.background[static_cast<size_t>(i)];
    tp.transparency = host.transparency ? 1 : 0;
    tp.curvesSet = host.curvesSet ? 1 : 0;
    tp.adaptiveDE = host.adaptiveDE ? 1 : 0;
    tp.deMinRadius = host.deMinRadius;
    tp.deMaxRadius = host.deMaxRadius;
    tp.deCurve = host.deCurve;
    tp.deMaxRadiusInt = host.deMaxRadiusInt;
    tp.filter = dFilter.get();
    tp.logScale = dLogScale.get();
    tp.curveTable = dCurveTable.get();
    tp.deKernelsFlat = dDeKernels.get();
    tp.deKernelOffsets = dDeOffsets.get();
    tp.useFloatBuckets = useFloatBuckets ? 1 : 0;
    tp.bucketsD = dBucketsD.get();
    tp.bucketsF = dBucketsF.get();
    tp.pixels = dPixels.get();

    const int totalPixels = host.width * host.height;
    const int tmBlocks = (totalPixels + kThreadsPerBlock - 1) / kThreadsPerBlock;
    tonemapKernel<<<tmBlocks, kThreadsPerBlock>>>(tp);
    if (cudaGetLastError() != cudaSuccess) return RenderedImage{};
    if (cudaDeviceSynchronize() != cudaSuccess) return RenderedImage{};

    const auto tToneMapEnd = clock::now();

    RenderedImage image;
    image.width = host.width;
    image.height = host.height;
    image.channels = host.channels;
    image.pixels.resize(static_cast<size_t>(host.width) * host.height * host.channels);
    if (!dPixels.downloadTo(image.pixels.data(), image.pixels.size())) return RenderedImage{};

    unsigned long long gen = 0, acc = 0;
    dPointsGenerated.downloadTo(&gen, 1);
    dPointsAccepted.downloadTo(&acc, 1);
    image.stats.pointsGenerated = gen + cpuStats.pointsGenerated;
    image.stats.pointsAccepted = acc + cpuStats.pointsAccepted;
    image.stats.cpuAssistPointsGenerated = cpuStats.pointsGenerated;

    if (timings) {
        timings->setupSeconds = secondsBetween(tStart, tSetupEnd);
        timings->iterateSeconds = secondsBetween(tSetupEnd, tIterateEnd);
        timings->reduceSeconds = secondsBetween(tIterateEnd, tReduceEnd);
        timings->tonemapSeconds = secondsBetween(tReduceEnd, tToneMapEnd);
        timings->gpuLastLaunchPointsPerSec = lastLaunchPointsPerSec;
    }
    return image;
}

} // namespace apo::gpu
