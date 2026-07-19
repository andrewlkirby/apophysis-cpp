// Tests for MemoryBudget.h/.cpp (FOLLOWUP_PLAN.txt B6) - the clamp-to-a-
// memory-budget logic RenderDialog/RenderAllDialog now apply before
// launching a full render. Deterministic and machine-independent: every
// test drives the explicit-totalPhysicalRamBytes overload of
// resolveMemorySafeThreadCount() with a fixed, known RAM figure rather than
// whatever's actually installed on the machine running the suite - see
// MemoryBudget.h's own doc comment on why that overload exists. Plain free
// functions with zero Qt dependency, but built/linked via apo_ui (where
// MemoryBudget.cpp lives) like every other file in this directory.

#include <QApplication>

#include "../TestHelpers.h"
#include "MemoryBudget.h"
#include "core/Flame.h"
#include "core/render/Renderer.h"

using apo_test::check;

namespace {

std::unique_ptr<apo::Flame> makeProbeFlame(int width = 800, int height = 600) {
    auto flame = std::make_unique<apo::Flame>();
    flame->width = width;
    flame->height = height;
    flame->spatialOversample = 1;
    return flame;
}

void testNoClampWhenRequestComfortablyFitsTheBudget() {
    auto flame = makeProbeFlame();
    // 64 GB of "RAM" against a normal-sized flame at a modest thread count -
    // nowhere close to the budget boundary.
    const auto ramBytes = static_cast<std::uint64_t>(64) * 1024 * 1024 * 1024;
    const apo::ui::ThreadBudgetResult result = apo::ui::resolveMemorySafeThreadCount(*flame, 4, ramBytes);

    check(!result.clamped, "a request that comfortably fits the budget is not clamped");
    check(result.threadCount == 4, "an unclamped result keeps the exact requested thread count");
    check(result.estimatedBytes == apo::Renderer::estimatePeakMemoryBytes(*flame, 4),
          "the returned estimate matches Renderer::estimatePeakMemoryBytes at the returned thread count");
}

void testClampsWhenRequestExceedsTheBudget() {
    auto flame = makeProbeFlame();
    const std::uint64_t perThreadBytes = apo::Renderer::estimatePeakMemoryBytes(*flame, 1);
    // Sized so 20 threads' worth of buckets is far more than this "RAM"
    // figure can hold (each additional thread is roughly one more
    // perThreadBytes - see estimatePeakMemoryBytes' own (threadCount+1)
    // scaling), but a handful of threads clearly fits.
    const std::uint64_t ramBytes = perThreadBytes * 6;
    const apo::ui::ThreadBudgetResult result = apo::ui::resolveMemorySafeThreadCount(*flame, 20, ramBytes);

    check(result.clamped, "a request whose estimate exceeds the budget is clamped");
    check(result.threadCount < 20, "the clamped thread count is below what was requested");
    check(result.threadCount >= 1, "the clamped thread count never drops below 1");
    check(result.estimatedBytes == apo::Renderer::estimatePeakMemoryBytes(*flame, result.threadCount),
          "the returned estimate matches Renderer::estimatePeakMemoryBytes at the clamped thread count");
    // The boundary itself: one more thread than the clamped result must NOT
    // have fit (otherwise the loop stopped a step too early).
    if (result.threadCount < 20) {
        const std::uint64_t oneMore = apo::Renderer::estimatePeakMemoryBytes(*flame, result.threadCount + 1);
        check(oneMore > ramBytes / 2, "one thread beyond the clamped count would have exceeded the budget");
    }
}

void testNeverGoesBelowOneThreadEvenWithAnAbsurdlyTinyBudget() {
    auto flame = makeProbeFlame();
    const apo::ui::ThreadBudgetResult result = apo::ui::resolveMemorySafeThreadCount(*flame, 16, /*ramBytes=*/1);

    check(result.threadCount == 1, "even a 1-byte budget clamps down to 1 thread, not 0 or negative");
    check(result.clamped, "reducing all the way to 1 thread still counts as clamped");
}

void testZeroRamMeansUnknownAndFailsOpen() {
    auto flame = makeProbeFlame();
    const apo::ui::ThreadBudgetResult result = apo::ui::resolveMemorySafeThreadCount(*flame, 16, /*ramBytes=*/0);

    check(!result.clamped, "totalPhysicalRamBytes=0 (unknown) never clamps - fails open, not closed");
    check(result.threadCount == 16, "an unknown RAM figure leaves the requested thread count untouched");
}

void testAutoRequestResolvesBeforeClamping() {
    auto flame = makeProbeFlame();
    // requestedThreadCount=0 (Auto) with an enormous RAM budget - should
    // resolve to a real positive hardware thread count (same convention as
    // Renderer::render()'s own threadCount=0), not stay 0.
    const auto ramBytes = static_cast<std::uint64_t>(1024) * 1024 * 1024 * 1024;
    const apo::ui::ThreadBudgetResult result = apo::ui::resolveMemorySafeThreadCount(*flame, 0, ramBytes);

    check(result.threadCount >= 1, "requestedThreadCount=0 (Auto) resolves to at least 1 real thread");
    check(!result.clamped, "an unclamped Auto request isn't flagged as clamped");
}

void testConvenienceOverloadQueriesRealMemoryWithoutCrashing() {
    // The two-argument overload (real callers - RenderDialog/
    // RenderAllDialog) queries this machine's actual RAM via
    // totalPhysicalMemoryBytes() internally - not asserting a specific
    // clamped/unclamped outcome (that depends on the real, unknown-at-
    // test-time machine), just that it runs to completion and returns a
    // sane, positive thread count for an ordinary flame/request.
    auto flame = makeProbeFlame();
    const apo::ui::ThreadBudgetResult result = apo::ui::resolveMemorySafeThreadCount(*flame, 4);
    check(result.threadCount >= 1 && result.threadCount <= 4,
          "the real-RAM overload returns a sane thread count between 1 and the request");
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testNoClampWhenRequestComfortablyFitsTheBudget();
    testClampsWhenRequestExceedsTheBudget();
    testNeverGoesBelowOneThreadEvenWithAnAbsurdlyTinyBudget();
    testZeroRamMeansUnknownAndFailsOpen();
    testAutoRequestResolvesBeforeClamping();
    testConvenienceOverloadQueriesRealMemoryWithoutCrashing();

    return apo_test::reportAndExit();
}
