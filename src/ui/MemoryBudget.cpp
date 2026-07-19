#include "MemoryBudget.h"

#include <algorithm>
#include <cstddef>
#include <thread>

#ifdef _WIN32
// NOMINMAX: without it, windows.h's own max()/min() macros silently
// shadow std::max/std::min below (a real, easy-to-hit trap - this file's
// first draft hit exactly this, MSVC's error pointing at the totally
// unrelated std::thread::hardware_concurrency() call next to std::max).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
// <sys/sysctl.h> isn't self-contained on macOS - it uses size_t internally
// but doesn't include the header that declares it, so it must come after
// <sys/types.h> or the build fails with "unknown type name 'size_t'"
// while parsing this system header itself.
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include "core/Flame.h"
#include "core/render/Renderer.h"

namespace apo::ui {

namespace {
// Half of physical RAM: leaves headroom for the OS, every other running
// application, and this render's own non-bucket allocations (output pixel
// buffer, tone-map lookup tables, the UI process itself) - all small next
// to the bucket arrays this is sized against, but real. Not user-tunable
// (no AppSettings entry) - this is a safety floor, not a preference; a user
// who wants to push closer to the edge can still raise the explicit thread
// count in Options, which resolveMemorySafeThreadCount() only ever reduces
// from, never overrides upward.
constexpr double kBudgetFractionOfPhysicalRam = 0.5;
} // namespace

std::uint64_t totalPhysicalMemoryBytes() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) return 0;
    return status.ullTotalPhys;
#elif defined(__APPLE__)
    std::uint64_t bytes = 0;
    std::size_t size = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) != 0) return 0;
    return bytes;
#elif defined(__linux__)
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages <= 0 || pageSize <= 0) return 0;
    return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(pageSize);
#else
    return 0; // unknown platform - fail open, see resolveMemorySafeThreadCount's own comment
#endif
}

ThreadBudgetResult resolveMemorySafeThreadCount(const apo::Flame& flame, int requestedThreadCount,
                                                 std::uint64_t totalPhysicalRamBytes) {
    ThreadBudgetResult result;
    result.threadCount = requestedThreadCount > 0
                              ? requestedThreadCount
                              : static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    result.estimatedBytes = apo::Renderer::estimatePeakMemoryBytes(flame, result.threadCount);

    if (totalPhysicalRamBytes == 0) return result; // unknown - fail open, see header comment

    const auto budget =
        static_cast<std::uint64_t>(static_cast<double>(totalPhysicalRamBytes) * kBudgetFractionOfPhysicalRam);
    while (result.threadCount > 1 && result.estimatedBytes > budget) {
        --result.threadCount;
        result.estimatedBytes = apo::Renderer::estimatePeakMemoryBytes(flame, result.threadCount);
        result.clamped = true;
    }
    return result;
}

ThreadBudgetResult resolveMemorySafeThreadCount(const apo::Flame& flame, int requestedThreadCount) {
    return resolveMemorySafeThreadCount(flame, requestedThreadCount, totalPhysicalMemoryBytes());
}

} // namespace apo::ui
