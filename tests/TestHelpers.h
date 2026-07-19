#pragma once

#include <cmath>
#include <cstdio>

namespace apo_test {

inline int& failureCount() {
    static int failures = 0;
    return failures;
}

// Returns `condition` (in addition to recording it) so a caller can
// short-circuit dependent checks - e.g. `if (!check(loaded, "...")) return;`
// before dereferencing something that only exists if the check passed.
// Existing call sites that use check() as a bare statement are unaffected:
// discarding a non-[[nodiscard]] return value is silent in C++.
inline bool check(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failureCount();
    } else {
        std::printf("ok: %s\n", description);
    }
    return condition;
}

inline bool approxEqual(double a, double b, double eps = 1e-9) { return std::fabs(a - b) < eps; }

// Call at the end of main() in every *_test.cpp file.
inline int reportAndExit() {
    if (failureCount() > 0) {
        std::fprintf(stderr, "\n%d check(s) failed\n", failureCount());
        return 1;
    }
    std::printf("\nall checks passed\n");
    return 0;
}

} // namespace apo_test
