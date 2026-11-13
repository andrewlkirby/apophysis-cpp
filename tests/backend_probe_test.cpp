// BackendProbe.h: the measurement behind RenderDialog's "Auto" render
// backend. Checks the pure decision rule and time estimate, that a CPU-only
// probe works without a GPU, that `abort` stops it early, and - when a CUDA
// device is available - that a GPU-sized render gets a GPU rate and a
// hybrid-sized CPU measurement, within a bounded time.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <thread>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/VariationRegistry.h"
#include "core/render/BackendProbe.h"
#include "core/render/RenderDispatcher.h"
#include "core/render/gpu/GpuAvailability.h"

using apo_test::check;

namespace {

using clock_t = std::chrono::steady_clock;
double secondsSince(clock_t::time_point t0) { return std::chrono::duration<double>(clock_t::now() - t0).count(); }

std::unique_ptr<apo::Flame> makeFlame(int width, int height, double density) {
    auto flame = std::make_unique<apo::Flame>();
    flame->width = width;
    flame->height = height;
    flame->sampleDensity = density;
    flame->center = {0.0, 0.0};
    flame->pixelsPerUnit = width / 4.0;
    flame->whiteLevel = 200;
    for (int i = 0; i < 256; ++i) flame->cmap.entries[i] = {i, i, i, 255};
    const int julian = apo::VariationRegistry::instance().variationIndex("julian");
    const int swirl = apo::VariationRegistry::instance().variationIndex("swirl");
    const double coefs[3][6] = {
        {0.7, -0.3, 0.3, 0.7, -0.2, 0.1},
        {0.6, 0.4, -0.4, 0.6, 0.3, -0.15},
        {-0.5, 0.5, 0.5, 0.5, -0.1, 0.25},
    };
    for (int i = 0; i < 3; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.color = i / 2.0;
        xf.c[0] = {coefs[i][0], coefs[i][3]};
        xf.c[1] = {coefs[i][1], coefs[i][4]};
        xf.c[2] = {coefs[i][2], coefs[i][5]};
        xf.setVariation(0, 0.0);
        xf.setVariation(swirl, 0.6);
        xf.setVariation(julian, 1.0);
    }
    return flame;
}

apo::BackendProbeResult fakeProbe(bool gpuUsable, double gpu, double cpu) {
    apo::BackendProbeResult r;
    r.valid = true;
    r.gpuUsable = gpuUsable;
    r.gpuPointsPerSec = gpu;
    r.cpuPointsPerSec = cpu;
    r.threadCount = 16;
    r.cpuThreads = gpuUsable ? 15 : 16;
    return r;
}

void testRecommendRule() {
    using apo::RenderBackend;
    check(apo::recommendBackend(fakeProbe(false, 0, 30e6)) == RenderBackend::Cpu, "rule: no usable GPU -> CPU");
    // The two measured cases this rule was set from (docs/FOLLOWUP_PLAN.txt).
    check(apo::recommendBackend(fakeProbe(true, 40e6, 39e6)) == RenderBackend::Hybrid,
          "rule: CPU about as fast as the GPU (divergent flame) -> GPU + CPU");
    check(apo::recommendBackend(fakeProbe(true, 800e6, 115e6)) == RenderBackend::Gpu,
          "rule: GPU much faster (simple flame) -> GPU only");
    const double g = 100e6;
    check(apo::recommendBackend(fakeProbe(true, g, apo::kHybridMinCpuToGpuRatio * g)) == RenderBackend::Hybrid,
          "rule: exactly at the threshold -> GPU + CPU");
}

void testEstimate() {
    using apo::RenderBackend;
    const auto r = fakeProbe(true, 40e6, 30e6); // cpu measured with 15 of 16 threads
    const std::uint64_t points = 7'000'000'000ULL;
    check(std::abs(apo::estimateRenderSeconds(r, RenderBackend::Hybrid, points) - 100.0) < 1e-6,
          "estimate: hybrid adds the two rates (7e9 / 70M = 100s)");
    check(std::abs(apo::estimateRenderSeconds(r, RenderBackend::Gpu, points) - 175.0) < 1e-6,
          "estimate: GPU only uses the GPU rate");
    check(std::abs(apo::estimateRenderSeconds(r, RenderBackend::Cpu, points) - 7e9 / (30e6 * 16 / 15)) < 1e-6,
          "estimate: CPU only scales the measured rate up to every thread");
    check(apo::estimateRenderSeconds(r, RenderBackend::Auto, points) ==
              apo::estimateRenderSeconds(r, RenderBackend::Hybrid, points),
          "estimate: Auto uses the recommended backend");
    check(apo::estimateRenderSeconds(fakeProbe(false, 0, 30e6), RenderBackend::Gpu, points) == 0,
          "estimate: 0 when the backend's rate is unknown");
}

void testCpuOnlyProbe() {
    auto flame = makeFlame(320, 240, 200.0);
    const auto t0 = clock_t::now();
    const apo::BackendProbeResult r =
        apo::probeBackends(*flame, 4, apo::BucketPrecision::Double, /*preferGpu=*/false);
    const double secs = secondsSince(t0);
    check(r.valid && !r.gpuUsable && r.gpuPointsPerSec == 0, "cpu-only probe: valid, GPU not used");
    check(r.cpuThreads == 4 && r.threadCount == 4, "cpu-only probe: measures with every requested thread");
    check(r.cpuPointsPerSec > 1e5, "cpu-only probe: measured a plausible CPU rate");
    check(apo::recommendBackend(r) == apo::RenderBackend::Cpu, "cpu-only probe: recommends CPU");
    char msg[128];
    std::snprintf(msg, sizeof(msg), "cpu-only probe: took %.2fs (< 2s)", secs);
    check(secs < 2.0, msg);
}

void testAbort() {
    auto flame = makeFlame(320, 240, 200.0);
    std::atomic<bool> abort{true};
    const auto t0 = clock_t::now();
    const apo::BackendProbeResult r =
        apo::probeBackends(*flame, 4, apo::BucketPrecision::Double, /*preferGpu=*/true, &abort);
    check(!r.valid, "abort: a pre-aborted probe reports invalid");
    check(secondsSince(t0) < 1.0, "abort: returns promptly");
}

void testGpuProbe() {
    if (!apo::gpu::isCudaAvailable()) {
        check(true, ("gpu probe skipped: " + apo::gpu::cudaUnavailableReason()).c_str());
        return;
    }
    auto flame = makeFlame(1280, 720, 400.0);
    if (!check(apo::RenderDispatcher::wouldUseGpu(*flame, true), "gpu probe: test flame is GPU-sized")) return;
    const int threads = std::max(2, static_cast<int>(std::thread::hardware_concurrency()));
    const auto t0 = clock_t::now();
    const apo::BackendProbeResult r = apo::probeBackends(*flame, threads, apo::BucketPrecision::Double, true);
    const double secs = secondsSince(t0);
    char msg[192];
    std::snprintf(msg, sizeof(msg), "gpu probe: valid, gpu %.1fM pts/s, cpu %.1fM pts/s, %.2fs", r.gpuPointsPerSec / 1e6,
                  r.cpuPointsPerSec / 1e6, secs);
    check(r.valid && r.gpuUsable && r.gpuPointsPerSec > 0 && r.cpuPointsPerSec > 0, msg);
    check(r.cpuThreads == threads - 1, "gpu probe: CPU measured with hybrid's thread count (one fewer)");
    check(secs < 6.0, "gpu probe: finishes within its time bounds");
}

} // namespace

int main() {
    testRecommendRule();
    testEstimate();
    testCpuOnlyProbe();
    testAbort();
    testGpuProbe();
    return apo_test::reportAndExit();
}
