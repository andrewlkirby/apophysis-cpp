// Hybrid CPU+GPU rendering (docs/HYBRID_CPU_GPU_RENDERING_PLAN.md): CPU
// worker threads iterate sub-batches alongside the GPU and stream their
// points to it. Checks that the result matches a GPU-only render of the same
// flame (statistically - the RNG streams differ), that every sub-batch runs
// exactly once across the two backends, that the CPU really contributes,
// and that progress/cancel/pause behave as on the GPU-only path.
//
// Skips (not fails) when no CUDA device is available or this binary wasn't
// built with APO_ENABLE_CUDA - same convention as gpu_progress_control_test.cpp.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/VariationRegistry.h"
#include "core/io/ImageCompare.h"
#include "core/render/RenderDispatcher.h"
#include "core/render/Renderer.h"
#include "core/render/gpu/DeviceFlame.h"
#include "core/render/gpu/GpuAvailability.h"

#ifdef APO_HAVE_CUDA
#include "core/render/gpu/GpuRenderer.h"
#endif

using apo_test::check;

namespace {

#ifdef APO_HAVE_CUDA
// Hybrid and GPU-only renders of the same flame use independent RNG streams
// but the same variation code for most points, so they agree closely
// (~57 dB measured at 640x480 d400). The bar is well above
// gpu_baseline_parity_test's 15 dB so that CPU points landing in the wrong
// buckets or with the wrong color would fail here.
constexpr double kMinPsnrDb = 35.0;

using clock_t = std::chrono::steady_clock;
double secondsSince(clock_t::time_point t0) { return std::chrono::duration<double>(clock_t::now() - t0).count(); }

int assistThreads() { return std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1); }

void setGrayscaleColorMap(apo::Flame& flame) {
    for (int i = 0; i < 256; ++i) {
        flame.cmap.entries[i][0] = static_cast<std::uint8_t>(i);
        flame.cmap.entries[i][1] = static_cast<std::uint8_t>(i);
        flame.cmap.entries[i][2] = static_cast<std::uint8_t>(i);
        flame.cmap.entries[i][3] = 255;
    }
}

std::unique_ptr<apo::Flame> baseFlame(int width, int height, double density) {
    auto flame = std::make_unique<apo::Flame>();
    flame->width = width;
    flame->height = height;
    flame->spatialOversample = 1;
    flame->sampleDensity = density;
    flame->center = {0.0, 0.0};
    flame->pixelsPerUnit = width / 4.0;
    flame->whiteLevel = 200;
    setGrayscaleColorMap(*flame);
    return flame;
}

// julian+swirl across 3 xforms - compute-heavy, the case hybrid helps most.
std::unique_ptr<apo::Flame> makeTranscendentalFlame(int width, int height, double density) {
    auto flame = baseFlame(width, height, density);
    auto& registry = apo::VariationRegistry::instance();
    const int julianIdx = registry.variationIndex("julian");
    const int swirlIdx = registry.variationIndex("swirl");
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
        xf.setVariation(swirlIdx, 0.6);
        xf.setVariation(julianIdx, 1.0);
    }
    return flame;
}

// Pure-affine Sierpinski-style IFS: contractive, never produces a
// non-finite point, so every sub-batch generates exactly kSubBatchSize
// points and pointsGenerated can be checked exactly.
std::unique_ptr<apo::Flame> makeLinearFlame(int width, int height, double density) {
    auto flame = baseFlame(width, height, density);
    const double offsets[3][2] = {{-0.5, -0.5}, {0.5, -0.5}, {0.0, 0.5}};
    for (int i = 0; i < 3; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.color = i / 2.0;
        xf.c[0] = {0.5, 0.0};
        xf.c[1] = {0.0, 0.5};
        xf.c[2] = {offsets[i][0], offsets[i][1]};
        xf.setVariation(0, 1.0);
    }
    return flame;
}

void checkPsnr(const apo::RenderedImage& a, const apo::RenderedImage& b, const char* label) {
    if (!check(!a.pixels.empty() && !b.pixels.empty() && a.width == b.width && a.height == b.height &&
                   a.channels == b.channels,
               label)) {
        return;
    }
    const double psnr = apo::computePsnr(a.width, a.height, a.channels, a.pixels.data(), b.pixels.data());
    char msg[192];
    std::snprintf(msg, sizeof(msg), "%s: hybrid vs GPU-only PSNR %.2f dB (>= %.1f)", label, psnr, kMinPsnrDb);
    check(psnr >= kMinPsnrDb, msg);
}

void testMatchesGpuOnly(apo::BucketPrecision precision, const char* label) {
    auto flame = makeTranscendentalFlame(640, 480, 400.0);
    const apo::RenderedImage gpuOnly = apo::gpu::GpuRenderer::render(*flame, /*seed=*/3, nullptr, nullptr, precision);
    const apo::RenderedImage hybrid =
        apo::gpu::GpuRenderer::render(*flame, /*seed=*/3, nullptr, nullptr, precision, assistThreads());
    checkPsnr(hybrid, gpuOnly, label);

    char msg[192];
    std::snprintf(msg, sizeof(msg), "%s: CPU produced %llu of %llu points (> 0 and < all)", label,
                  static_cast<unsigned long long>(hybrid.stats.cpuAssistPointsGenerated),
                  static_cast<unsigned long long>(hybrid.stats.pointsGenerated));
    check(hybrid.stats.cpuAssistPointsGenerated > 0 &&
              hybrid.stats.cpuAssistPointsGenerated < hybrid.stats.pointsGenerated,
          msg);
    check(gpuOnly.stats.cpuAssistPointsGenerated == 0, "GPU-only render reports no CPU-assist points");
}

// Every sub-batch must run exactly once across both backends: none dropped
// at the CPU/GPU handoff, none claimed twice.
void testEverySubBatchRunsOnce() {
    auto flame = makeLinearFlame(800, 600, 200.0);
    apo::RenderProgress progress;
    const apo::RenderedImage img =
        apo::gpu::GpuRenderer::render(*flame, /*seed=*/5, &progress, nullptr, apo::BucketPrecision::Double,
                                      assistThreads());
    const std::uint64_t target = progress.pointsTarget.load();
    char msg[192];
    std::snprintf(msg, sizeof(msg), "sub-batches: pointsGenerated %llu == pointsTarget %llu",
                  static_cast<unsigned long long>(img.stats.pointsGenerated),
                  static_cast<unsigned long long>(target));
    check(target > 0 && img.stats.pointsGenerated == target, msg);
    check(progress.pointsDone.load() == target, "sub-batches: pointsDone reaches exactly pointsTarget");
    check(img.stats.cpuAssistPointsGenerated > 0, "sub-batches: the CPU workers claimed some sub-batches");
}

void testDispatcher() {
    auto flame = makeTranscendentalFlame(640, 480, 400.0);
    check(apo::RenderDispatcher::wouldUseCpuAssist(*flame, /*preferGpu=*/true, /*cpuAssist=*/true),
          "dispatch: wouldUseCpuAssist for a GPU-sized render with cpuAssist on");
    check(!apo::RenderDispatcher::wouldUseCpuAssist(*flame, /*preferGpu=*/true, /*cpuAssist=*/false),
          "dispatch: no CPU assist when the option is off");
    check(!apo::RenderDispatcher::wouldUseCpuAssist(*flame, /*preferGpu=*/false, /*cpuAssist=*/true),
          "dispatch: no CPU assist when GPU rendering is off");
    check(!apo::RenderDispatcher::wouldUseCpuAssist(*flame, true, true, /*threadCount=*/1),
          "dispatch: no CPU assist with a single thread (none to spare)");

    bool usedGpu = false;
    const apo::RenderedImage img =
        apo::RenderDispatcher::render(*flame, /*seed=*/9, /*threadCount=*/0, nullptr, nullptr,
                                      apo::BucketPrecision::Double, /*preferGpu=*/true, &usedGpu, /*cpuAssist=*/true);
    check(usedGpu && !img.pixels.empty(), "dispatch: hybrid render reports usedGpu and returns an image");
    check(img.stats.cpuAssistPointsGenerated > 0, "dispatch: hybrid render through the dispatcher used the CPU");
}

void testCancel() {
    auto flame = makeTranscendentalFlame(1920, 1080, 400.0);
    apo::RenderProgress progress;
    apo::RenderedImage img;
    std::thread renderThread([&] {
        img = apo::gpu::GpuRenderer::render(*flame, /*seed=*/11, &progress, nullptr, apo::BucketPrecision::Double,
                                            assistThreads());
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    const auto tCancel = clock_t::now();
    progress.cancelRequested.store(true);
    renderThread.join();
    const double latency = secondsSince(tCancel);

    check(progress.pointsDone.load() < progress.pointsTarget.load(), "cancel: hybrid render stops short of target");
    check(!img.pixels.empty(), "cancel: a cancelled hybrid render still returns its partial image");
    char msg[128];
    std::snprintf(msg, sizeof(msg), "cancel: latency %.3fs (< 2.0s)", latency);
    check(latency < 2.0, msg);
}

void testPauseThenResume() {
    auto flame = makeTranscendentalFlame(1920, 1080, 400.0);
    apo::RenderProgress progress;
    std::thread renderThread([&] {
        apo::gpu::GpuRenderer::render(*flame, /*seed=*/12, &progress, nullptr, apo::BucketPrecision::Double,
                                      assistThreads());
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    progress.pauseRequested.store(true);

    // Same settle logic as gpu_progress_control_test: frozen = unchanged for
    // a full second, which is longer than any one GPU launch.
    std::uint64_t lastSeen = progress.pointsDone.load();
    auto tLastChange = clock_t::now();
    const auto tPause = clock_t::now();
    bool settled = false;
    while (secondsSince(tPause) < 6.0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const std::uint64_t now = progress.pointsDone.load();
        if (now != lastSeen) {
            lastSeen = now;
            tLastChange = clock_t::now();
        } else if (secondsSince(tLastChange) >= 1.0) {
            settled = true;
            break;
        }
    }
    if (!check(settled, "pause: hybrid render's pointsDone freezes while paused")) {
        progress.cancelRequested.store(true);
        renderThread.join();
        return;
    }
    check(lastSeen < progress.pointsTarget.load(), "pause: froze before completion");

    progress.pauseRequested.store(false);
    renderThread.join();
    check(progress.pointsDone.load() == progress.pointsTarget.load(),
          "pause: resumed hybrid render reaches completion");
}
#endif

} // namespace

int main() {
    if (!apo::gpu::isCudaAvailable()) {
        check(true, ("skipped: " + apo::gpu::cudaUnavailableReason()).c_str());
        return apo_test::reportAndExit();
    }

#ifdef APO_HAVE_CUDA
    testMatchesGpuOnly(apo::BucketPrecision::Double, "double");
    testMatchesGpuOnly(apo::BucketPrecision::Float, "float");
    testEverySubBatchRunsOnce();
    testDispatcher();
    testCancel();
    testPauseThenResume();
#endif

    return apo_test::reportAndExit();
}
