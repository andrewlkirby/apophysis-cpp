#include "AutoFrame.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
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

// hasMinimumColoredCoverage's own test-render settings. 64x64 at
// sampleDensity=10 is ~41000 points - cheap (same order of magnitude as
// this file's own kSampleCount above), but plenty to estimate coverage
// down to the ~1% scale kDefaultMinColoredCoverage cares about.
constexpr int kCoverageTestSize = 64;
constexpr double kCoverageTestSampleDensity = 10.0;
// Sum of |R-bgR|+|G-bgG|+|B-bgB| (out of a possible 765) a pixel must
// exceed to count as "covered" rather than background - small enough to
// catch faint anti-aliased edges, large enough to ignore the kind of
// single-LSB rounding noise a gamma/tone-map curve can leave even on an
// otherwise-untouched background pixel.
constexpr int kCoverageColorDeltaThreshold = 24;
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

bool hasMinimumColoredCoverage(const Flame& flame, std::uint64_t seed, double minCoverageFraction) {
    if (flame.width <= 0 || flame.height <= 0) return true;
    if (minCoverageFraction <= 0.0) return true;

    auto testFlame = flame.clone();
    // Fits into a kCoverageTestSize x kCoverageTestSize box aspect-
    // correctly, then rescales pixelsPerUnit to preserve the exact same
    // on-screen composition (Flame::adjustScale's own contract) - a plain
    // width/height reassignment would leave pixelsPerUnit computed for the
    // original (usually much larger) canvas untouched, zooming into a
    // tighter crop instead of shrinking the same view (the same bug
    // ThumbnailTask.h's own thumbnails had before switching to this same
    // fit-then-adjustScale pattern).
    const double aspect = flame.height > 0 ? static_cast<double>(flame.width) / flame.height : 1.0;
    int tw = kCoverageTestSize, th = kCoverageTestSize;
    if (aspect > 1.0) {
        th = std::max(1, static_cast<int>(kCoverageTestSize / aspect));
    } else {
        tw = std::max(1, static_cast<int>(kCoverageTestSize * aspect));
    }
    testFlame->adjustScale(tw, th);
    testFlame->spatialOversample = 1;
    testFlame->sampleDensity = kCoverageTestSampleDensity;
    // Raw hit coverage, not blurred - density estimation would smear a
    // tiny cluster of hits into looking like more coverage than it has,
    // exactly the case this function exists to catch.
    testFlame->enableDE = false;

    // threadCount=1: this runs inside a discard/retry loop (up to
    // kMaxDiscardBlankRetries attempts per batch slot in MainWindow.cpp),
    // so avoiding thread-pool spin-up/join overhead on a render this
    // small/cheap matters more than parallelizing it.
    const RenderedImage image = Renderer::render(*testFlame, seed, /*threadCount=*/1);
    if (image.pixels.empty() || image.width <= 0 || image.height <= 0) return false;

    const auto channels = static_cast<size_t>(image.channels);
    const int totalPixels = image.width * image.height;
    int coveredPixels = 0;
    for (int i = 0; i < totalPixels; ++i) {
        const std::uint8_t* px = &image.pixels[static_cast<size_t>(i) * channels];
        const int delta = std::abs(static_cast<int>(px[0]) - flame.background[0]) +
                           std::abs(static_cast<int>(px[1]) - flame.background[1]) +
                           std::abs(static_cast<int>(px[2]) - flame.background[2]);
        if (delta > kCoverageColorDeltaThreshold) ++coveredPixels;
    }

    return static_cast<double>(coveredPixels) / static_cast<double>(totalPixels) >= minCoverageFraction;
}

} // namespace apo
