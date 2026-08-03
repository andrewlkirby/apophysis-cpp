#include "GpuRenderer.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include <cuda_runtime.h>

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

// Sub-batches (chaos-kernel threads) launched per chunk. Each is a full
// kDeviceSubBatchSize-point unit of work (see DeviceTypes.h's mapping
// comment), so this is also the granularity progress/cancel/pause polling
// happens at between launches - a compromise between launch overhead
// (bigger is better) and interactivity (smaller is better), same role as
// the CPU renderer's per-sub-batch check in Renderer.cpp's renderImpl().
constexpr int kBatchesPerLaunch = 4096;
constexpr int kThreadsPerBlock = 256;

void waitWhilePaused(RenderProgress* progress) {
    if (!progress) return;
    while (progress->pauseRequested.load(std::memory_order_relaxed) &&
           !progress->cancelRequested.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

} // namespace

RenderedImage GpuRenderer::render(const Flame& flame, std::uint64_t seed, RenderProgress* progress,
                                   RenderTimings* timings, BucketPrecision precision) {
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

    const auto tSetupEnd = clock::now();

    for (int batchOffset = 0; batchOffset < host.numBatches; batchOffset += kBatchesPerLaunch) {
        if (progress && progress->cancelRequested.load(std::memory_order_relaxed)) break;
        waitWhilePaused(progress);
        if (progress && progress->cancelRequested.load(std::memory_order_relaxed)) break;

        const int threadsThisLaunch = std::min(kBatchesPerLaunch, host.numBatches - batchOffset);
        const int blocks = (threadsThisLaunch + kThreadsPerBlock - 1) / kThreadsPerBlock;
        chaosKernel<<<blocks, kThreadsPerBlock>>>(kp, seed, batchOffset, threadsThisLaunch, dPointsGenerated.get(),
                                                    dPointsAccepted.get());
        if (cudaGetLastError() != cudaSuccess) return RenderedImage{};
        if (cudaDeviceSynchronize() != cudaSuccess) return RenderedImage{};

        if (progress) {
            progress->pointsDone.fetch_add(static_cast<std::uint64_t>(threadsThisLaunch) * kDeviceSubBatchSize,
                                            std::memory_order_relaxed);
        }
    }

    const auto tIterateEnd = clock::now();
    // No separate reduction pass on this backend - every thread atomicAdd's
    // straight into the shared device histogram during iteration (see
    // DeviceTypes.h's mapping comment), unlike the CPU renderer's
    // private-per-thread-bucket-then-reduce design.
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
    image.stats.pointsGenerated = gen;
    image.stats.pointsAccepted = acc;

    if (timings) {
        timings->setupSeconds = secondsBetween(tStart, tSetupEnd);
        timings->iterateSeconds = secondsBetween(tSetupEnd, tIterateEnd);
        timings->reduceSeconds = secondsBetween(tIterateEnd, tReduceEnd);
        timings->tonemapSeconds = secondsBetween(tReduceEnd, tToneMapEnd);
    }
    return image;
}

} // namespace apo::gpu
