#include "AutoFrame.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "Renderer.h"

namespace apo {

namespace {
// Matches ControlPoint.pas's SUB_BATCH_SIZE-sized sample and its
// LimitOutSidePoints = 5% trim fraction (verified directly against
// source), just applied as a direct percentile computation instead of an
// iterative bisection converging toward the same target.
constexpr int kSampleCount = 10000;
constexpr double kTrimFraction = 0.05;
} // namespace

bool autoFrameFlame(Flame& flame, std::uint64_t seed, int minValidSamples) {
    if (flame.width <= 0 || flame.height <= 0) return false;

    const auto samples = Renderer::samplePoints(flame, seed, kSampleCount);
    // Too few points to make a reasonable estimate (a near-fully-degenerate
    // flame) - leave center/pixelsPerUnit exactly as they were rather than
    // computing a framing from noise.
    if (static_cast<int>(samples.size()) < minValidSamples) return false;

    std::vector<double> xs, ys;
    xs.reserve(samples.size());
    ys.reserve(samples.size());
    for (const auto& [x, y] : samples) {
        xs.push_back(x);
        ys.push_back(y);
    }

    const size_t lowIdx = static_cast<size_t>(static_cast<double>(xs.size()) * kTrimFraction);
    const size_t highIdx = xs.size() - 1 - lowIdx;

    std::nth_element(xs.begin(), xs.begin() + static_cast<long>(lowIdx), xs.end());
    const double xLow = xs[lowIdx];
    std::nth_element(xs.begin(), xs.begin() + static_cast<long>(highIdx), xs.end());
    const double xHigh = xs[highIdx];
    std::nth_element(ys.begin(), ys.begin() + static_cast<long>(lowIdx), ys.end());
    const double yLow = ys[lowIdx];
    std::nth_element(ys.begin(), ys.begin() + static_cast<long>(highIdx), ys.end());
    const double yHigh = ys[highIdx];

    const double deltaX = xHigh - xLow;
    const double deltaY = yHigh - yLow;

    // CalcBoundbox's own fallback for an implausibly large (>1000-unit)
    // attractor extent forced center=(0,0) and pixelsPerUnit=10 here - but
    // that's usually nowhere near the actual (trimmed) attractor bounds
    // above, so it usually rendered blank anyway. Treated as degenerate
    // instead (see this function's own doc comment).
    if (deltaX > 1000 || deltaY > 1000) return false;

    // CalcBoundbox's own near-zero-area fallback (0.65 fill-fraction
    // constant verified directly against source) used to default to
    // pixelsPerUnit=10 here too - but a collapsed-to-(near)-a-single-point
    // attractor is visually indistinguishable from blank at any zoom.
    // Treated as degenerate instead (see this function's own doc comment).
    if (deltaX <= 0.001 || deltaY <= 0.001) return false;

    flame.center = {(xLow + xHigh) / 2.0, (yLow + yHigh) / 2.0};
    flame.pixelsPerUnit = 0.65 * std::min(flame.width / deltaX, flame.height / deltaY);
    return true;
}

} // namespace apo
