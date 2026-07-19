#pragma once

#include <cstdint>

namespace apo {
class Flame;
}

namespace apo::ui {

// FOLLOWUP_PLAN.txt B6. Renderer::estimatePeakMemoryBytes() already tells a
// caller how much memory a render at a given thread count will use, and
// RenderDialog already surfaces that estimate to the user - but nothing
// acted on it: at a large enough size/oversample with enough threads, the
// estimate can exceed physical RAM by many times over, and the OS paging
// the difference to disk is a performance catastrophe far worse than simply
// running fewer threads (the whole point of using more threads in the first
// place). This is the piece that acts on the estimate.
struct ThreadBudgetResult {
    // The thread count to actually pass to Renderer::render()/
    // RenderWorker::renderFull - always >= 1, and already resolved from
    // "0 means Auto" to a concrete number (Renderer::estimatePeakMemoryBytes
    // needs a concrete count to estimate against, so there's no reason for
    // the caller to resolve 0 a second time).
    int threadCount = 1;
    // True if threadCount had to be reduced below what was requested to fit
    // the memory budget - callers should surface this to the user (e.g. a
    // status-line note) rather than silently rendering slower than asked.
    bool clamped = false;
    // Renderer::estimatePeakMemoryBytes(flame, threadCount) at the
    // *returned* threadCount - callers that already want to show a memory
    // estimate (RenderDialog's memoryEstimateLabel_) can reuse this instead
    // of recomputing it.
    std::uint64_t estimatedBytes = 0;
};

// Resolves `requestedThreadCount` (same 0=Auto/N=explicit convention as
// Renderer::render() and AppSettings::renderThreadCount()) down to a thread
// count whose estimated peak memory fits within a budget fraction of
// `totalPhysicalRamBytes` - reducing one thread at a time and re-estimating
// via the real Renderer::estimatePeakMemoryBytes() formula each step (not a
// hand-derived shortcut, so this can never drift out of sync with that
// formula as it evolves), stopping as soon as it fits or reaching 1 thread.
// Never returns 0 - a single thread's own estimate is the theoretical
// minimum for this render regardless of thread count, so if even that
// doesn't fit the budget, this returns 1 thread anyway (flagged as
// clamped) rather than refusing to render at all; a slow, possibly-
// thrashing render the user explicitly asked for beats silently doing
// nothing.
//
// totalPhysicalRamBytes == 0 (e.g. totalPhysicalMemoryBytes() couldn't
// determine it) fails open - no clamping at all - rather than guessing at
// a budget with no real number behind it. Takes the RAM figure as an
// explicit parameter (rather than always querying it internally) so tests
// can exercise the clamping arithmetic itself against a known, fixed
// figure instead of whatever RAM happens to be installed in the machine
// running the test suite - see the convenience overload below for what
// every real UI call site actually wants.
ThreadBudgetResult resolveMemorySafeThreadCount(const apo::Flame& flame, int requestedThreadCount,
                                                 std::uint64_t totalPhysicalRamBytes);

// Convenience overload for real callers: queries the machine's actual
// physical RAM via totalPhysicalMemoryBytes() below.
ThreadBudgetResult resolveMemorySafeThreadCount(const apo::Flame& flame, int requestedThreadCount);

// Total physical RAM in bytes (Windows: GlobalMemoryStatusEx, macOS:
// sysctlbyname("hw.memsize"), Linux: sysconf(_SC_PHYS_PAGES) *
// sysconf(_SC_PAGE_SIZE)). Returns 0 on any platform/failure this port
// doesn't have a query for - see resolveMemorySafeThreadCount()'s own
// fail-open contract for that case.
std::uint64_t totalPhysicalMemoryBytes();

} // namespace apo::ui
