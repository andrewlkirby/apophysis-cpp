// Progress/cancel/pause tuning for the CUDA render backend under real
// interactive-scale load (docs/GPU_RENDERING_PLAN.md's "Polish" phase) - the
// GPU analogue of renderer_test.cpp's testProgressReportsTargetAndReaches
// Completion/testCancellationStopsRenderEarly/testPauseBlocksProgressUntil
// Resumed/testPauseThenCancelStopsWithoutNeedingResumeFirst, but exercised at
// the point-count scale a real interactive preview or final render actually
// hits, not a single-sub-batch toy case: GpuRenderer::render() polls
// progress/cancel/pause once per kernel-launch chunk, sized by
// GpuRenderer.cu's adaptLaunchSize() to target ~0.35s per launch (see that
// function's own doc comment) rather than a fixed sub-batch count - a
// coarser granularity than the CPU renderer's per-10,000-point check, so
// this file specifically measures *how coarse in practice* - the time
// between setting cancelRequested/pauseRequested and the render actually
// responding - against RenderDialog.cpp's own kProgressPollIntervalMs=150ms
// UI polling cadence, plus how many distinct progress ticks a multi-second
// GPU render actually produces (a progress bar that jumps 0%->100% in one
// step would be a real regression even though pointsDone/pointsTarget are
// individually "correct").
//
// Skips (not fails) entirely when no CUDA device is available, or when this
// binary wasn't built with APO_ENABLE_CUDA - same convention as
// gpu_variation_parity_test.cpp.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/VariationRegistry.h"
#include "core/render/Renderer.h"
#include "core/render/gpu/DeviceFlame.h"
#include "core/render/gpu/GpuAvailability.h"

#ifdef APO_HAVE_CUDA
#include "core/render/gpu/GpuRenderer.h"
#endif

using apo_test::check;

namespace {

void setGrayscaleColorMap(apo::Flame& flame) {
    for (int i = 0; i < 256; ++i) {
        flame.cmap.entries[i][0] = static_cast<std::uint8_t>(i);
        flame.cmap.entries[i][1] = static_cast<std::uint8_t>(i);
        flame.cmap.entries[i][2] = static_cast<std::uint8_t>(i);
        flame.cmap.entries[i][3] = 255;
    }
}

// julian+swirl across 3 xforms - same fixture shape as apo_bench.cpp's
// makeTranscendentalFlame (transcendental-math-heavy, representative of a
// real user's flame rather than a pure-affine toy case). Parameterized by
// width/height/density so the same builder covers both the "interactive
// preview" and "final render" scales below.
std::unique_ptr<apo::Flame> buildInteractiveFlame(int width, int height, double density) {
    auto flame = std::make_unique<apo::Flame>();
    flame->width = width;
    flame->height = height;
    flame->spatialOversample = 1;
    flame->sampleDensity = density;
    flame->center = {0.0, 0.0};
    flame->pixelsPerUnit = width / 4.0;
    flame->whiteLevel = 200;
    setGrayscaleColorMap(*flame);

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

#ifdef APO_HAVE_CUDA
using clock_t = std::chrono::steady_clock;
double secondsSince(clock_t::time_point t0) {
    return std::chrono::duration<double>(clock_t::now() - t0).count();
}

// ~640x480 @ density 400: ~123M points, ~3 GPU launch chunks - roughly the
// cost of a live editor preview render.
std::unique_ptr<apo::Flame> makePreviewScaleFlame() { return buildInteractiveFlame(640, 480, 400.0); }

// ~1920x1080 @ density 400: ~829M points - roughly the cost of a real
// final-export render, long enough (several seconds on a mid-range consumer
// GPU, spread over several adaptively-sized launch chunks) to observe
// multiple in-flight launches and meaningfully exercise mid-render
// pause/cancel.
std::unique_ptr<apo::Flame> makeFinalScaleFlame() { return buildInteractiveFlame(1920, 1080, 400.0); }

void testGpuProgressReachesCompletion() {
    auto flame = makePreviewScaleFlame();
    if (!check(apo::gpu::isFlameGpuEligible(*flame), "progress: preview-scale test flame is GPU-eligible")) return;

    apo::RenderProgress progress;
    check(progress.pointsTarget.load() == 0, "progress: pointsTarget starts at 0 before render() is called");

    apo::RenderedImage img = apo::gpu::GpuRenderer::render(*flame, /*seed=*/7, &progress);

    check(progress.pointsTarget.load() > 0, "progress: GPU render() sets a nonzero pointsTarget");
    check(progress.pointsDone.load() == progress.pointsTarget.load(),
          "progress: an uncancelled GPU render's pointsDone reaches exactly pointsTarget by completion");
    check(!img.pixels.empty(), "progress: the GPU render still produces a real image when a progress token is supplied");
}

// Mirrors renderer_test.cpp's testCancellationStopsRenderEarly, but at
// final-render scale and measuring how long cancellation actually takes to
// take effect (bounded by GpuRenderer.cu's per-launch-chunk poll cadence),
// not just that it eventually does.
void testGpuCancellationLatency() {
    auto flame = makeFinalScaleFlame();
    if (!check(apo::gpu::isFlameGpuEligible(*flame), "cancel: final-scale test flame is GPU-eligible")) return;

    apo::RenderProgress progress;
    std::thread renderThread(
        [&] { apo::gpu::GpuRenderer::render(*flame, /*seed=*/11, &progress); });

    // Let a few launch chunks complete first, so cancellation genuinely
    // interrupts an in-progress render rather than racing its very start.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const auto tCancelRequested = clock_t::now();
    progress.cancelRequested.store(true);
    renderThread.join();
    const double latencySeconds = secondsSince(tCancelRequested);

    check(progress.pointsDone.load() < progress.pointsTarget.load(),
          "cancel: a cancelled final-scale GPU render stops with pointsDone short of pointsTarget");

    char msg[192];
    std::snprintf(msg, sizeof(msg),
                  "cancel: latency from cancelRequested to render thread joining was %.3fs (< 2.0s bound, vs "
                  "RenderDialog's 0.150s UI poll interval)",
                  latencySeconds);
    check(latencySeconds < 2.0, msg);
}

// Mirrors renderer_test.cpp's testPauseBlocksProgressUntilResumed, at
// final-render scale: measures the latency between requesting a pause and
// pointsDone actually freezing, confirms it then stays frozen (not just
// coincidentally caught between two equal polls), and confirms clearing the
// request lets the render reach completion normally.
void testGpuPauseLatencyThenResume() {
    auto flame = makeFinalScaleFlame();
    if (!check(apo::gpu::isFlameGpuEligible(*flame), "pause: final-scale test flame is GPU-eligible")) return;

    apo::RenderProgress progress;
    std::thread renderThread(
        [&] { apo::gpu::GpuRenderer::render(*flame, /*seed=*/12, &progress); });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const auto tPauseRequested = clock_t::now();
    progress.pauseRequested.store(true);

    // pointsDone only moves in one lump per launch chunk (updated after
    // cudaDeviceSynchronize() returns - see GpuRenderer.cu's launch loop),
    // not continuously while a chunk's kernel runs, and a chunk can easily
    // take longer than a couple of poll intervals - so two consecutive 20ms
    // reads agreeing is NOT proof the render is paused, only that no
    // increment landed in that particular 20ms window while one may still
    // be in flight. Instead: keep polling and reset the "stable since"
    // clock on every observed change; only declare settled once the value
    // has held for a full second straight (safely longer than any single
    // chunk's duration, per the cancel-latency measurement above), bounded
    // so a real regression (pause never taking effect) fails instead of
    // hanging.
    std::uint64_t lastSeen = progress.pointsDone.load();
    auto tLastChange = clock_t::now();
    double settleLatency = -1.0;
    while (secondsSince(tPauseRequested) < 6.0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const std::uint64_t now = progress.pointsDone.load();
        if (now != lastSeen) {
            lastSeen = now;
            tLastChange = clock_t::now();
        } else if (secondsSince(tLastChange) >= 1.0) {
            settleLatency = std::chrono::duration<double>(tLastChange - tPauseRequested).count();
            break;
        }
    }

    char settleMsg[192];
    std::snprintf(settleMsg, sizeof(settleMsg),
                  "pause: latency from pauseRequested to pointsDone freezing was %.3fs (< 2.0s bound)",
                  settleLatency);
    if (!check(settleLatency >= 0.0 && settleLatency < 2.0, settleMsg)) {
        progress.cancelRequested.store(true); // don't leave the render thread stuck paused forever
        renderThread.join();
        return;
    }

    const std::uint64_t frozenAt = lastSeen;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    check(progress.pointsDone.load() == frozenAt,
          "pause: pointsDone stays frozen for as long as the GPU render stays paused");

    progress.pauseRequested.store(false);
    renderThread.join();

    check(progress.pointsDone.load() == progress.pointsTarget.load(),
          "pause: clearing pauseRequested lets a paused GPU render reach completion normally");
}

// Mirrors renderer_test.cpp's testPauseThenCancelStopsWithoutNeedingResumeFirst.
void testGpuPauseThenCancelNoResumeNeeded() {
    auto flame = makeFinalScaleFlame();
    if (!check(apo::gpu::isFlameGpuEligible(*flame), "pause+cancel: final-scale test flame is GPU-eligible")) return;

    apo::RenderProgress progress;
    std::thread renderThread(
        [&] { apo::gpu::GpuRenderer::render(*flame, /*seed=*/13, &progress); });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    progress.pauseRequested.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // let it actually settle into paused
    progress.cancelRequested.store(true); // cancel while still paused, without clearing pauseRequested first
    renderThread.join(); // must not hang waiting for a resume that never comes

    check(progress.pointsDone.load() < progress.pointsTarget.load(),
          "pause+cancel: a GPU render cancelled while paused stops short, without needing Resume first");
}

// Counts how many distinct pointsDone values a UI-style poller (finer than
// RenderDialog's own 150ms interval, so it doesn't itself become the
// bottleneck being measured) observes over the course of a multi-second
// final-scale render - the "does the progress bar actually move" check that
// pointsDone==pointsTarget-at-completion alone doesn't cover.
void testGpuProgressTickCountOverLongRender() {
    auto flame = makeFinalScaleFlame();
    if (!check(apo::gpu::isFlameGpuEligible(*flame), "ticks: final-scale test flame is GPU-eligible")) return;

    apo::RenderProgress progress;
    std::thread renderThread(
        [&] { apo::gpu::GpuRenderer::render(*flame, /*seed=*/17, &progress); });

    int distinctTicks = 0;
    std::uint64_t lastSeen = 0;
    const auto tStart = clock_t::now();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const std::uint64_t now = progress.pointsDone.load();
        if (now != lastSeen) {
            ++distinctTicks;
            lastSeen = now;
        }
        if (now >= progress.pointsTarget.load() && progress.pointsTarget.load() > 0) break;
        if (secondsSince(tStart) > 30.0) break; // safety valve, not expected to be hit
    }
    renderThread.join();
    const double wallSeconds = secondsSince(tStart);

    char msg[192];
    std::snprintf(msg, sizeof(msg),
                  "ticks: observed %d distinct progress updates over a %.3fs final-scale GPU render "
                  "(>= 3 expected - a responsive progress bar, not a single 0%%->100%% jump)",
                  distinctTicks, wallSeconds);
    check(distinctTicks >= 3, msg);
}
#endif

} // namespace

int main() {
    if (!apo::gpu::isCudaAvailable()) {
        check(true, ("skipped: " + apo::gpu::cudaUnavailableReason()).c_str());
        return apo_test::reportAndExit();
    }

#ifdef APO_HAVE_CUDA
    testGpuProgressReachesCompletion();
    testGpuCancellationLatency();
    testGpuPauseLatencyThenResume();
    testGpuPauseThenCancelNoResumeNeeded();
    testGpuProgressTickCountOverLongRender();
#endif

    return apo_test::reportAndExit();
}
