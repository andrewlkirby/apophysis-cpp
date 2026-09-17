#include "AutoFrame.h"

#include <algorithm>
#include <cmath>
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

// measureContent's own test-render settings. 128x128 at sampleDensity=20 is
// ~330000 points - still cheap next to a real render, but a big step up
// from the 64x64/~41000-point version this replaced: at 64x64, one test
// pixel spans roughly 20x12 pixels of a typical 1280x800 output, so even
// thin/sparse structure "touches" nearly every test cell and reads as far
// more covered than the real render looks. 128x128 quarters that per-cell
// footprint, tracking perceived fullness much more closely.
constexpr int kCoverageTestSize = 128;
constexpr double kCoverageTestSampleDensity = 20.0;
// Luminance (Rec.601, 0-255) delta from the background's own luminance a
// pixel must exceed to count as "covered" - small enough to catch faint
// anti-aliased edges, large enough to ignore single-LSB rounding noise a
// gamma/tone-map curve can leave even on an otherwise-untouched background
// pixel. Luminance rather than a raw per-channel RGB sum so the threshold
// means the same thing regardless of hue, and so it composes directly with
// meanForegroundLum/lumStdDev below (all three drawn from the same measure).
constexpr double kLumDeltaThreshold = 8.0;
// score's own brightness/contrast normalization: meanForegroundLum and
// lumStdDev values at/above these (0-255) saturate their respective score
// terms to 1.0. Not exposed as settings - see measureContent's own doc
// comment on why these stay implementation constants.
constexpr double kLumReference = 64.0;
constexpr double kContrastReference = 48.0;

double luminance(int r, int g, int b) {
    return 0.299 * r + 0.587 * g + 0.114 * b;
}
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
    return measureContent(flame, seed).coveredFraction >= minCoverageFraction;
}

ContentScore measureContent(const Flame& flame, std::uint64_t seed) {
    if (flame.width <= 0 || flame.height <= 0) return {};

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
    if (image.pixels.empty() || image.width <= 0 || image.height <= 0) return {};

    const auto channels = static_cast<size_t>(image.channels);
    const int totalPixels = image.width * image.height;
    const double bgLum = luminance(flame.background[0], flame.background[1], flame.background[2]);
    int coveredPixels = 0;
    double sumFgLum = 0.0;
    double sumFgLum2 = 0.0;
    for (int i = 0; i < totalPixels; ++i) {
        const std::uint8_t* px = &image.pixels[static_cast<size_t>(i) * channels];
        const double lum = luminance(px[0], px[1], px[2]);
        if (std::abs(lum - bgLum) > kLumDeltaThreshold) {
            ++coveredPixels;
            sumFgLum += lum;
            sumFgLum2 += lum * lum;
        }
    }

    ContentScore out;
    out.coveredFraction = static_cast<double>(coveredPixels) / static_cast<double>(totalPixels);
    if (coveredPixels > 0) {
        out.meanForegroundLum = sumFgLum / coveredPixels;
        const double variance =
            std::max(0.0, sumFgLum2 / coveredPixels - out.meanForegroundLum * out.meanForegroundLum);
        out.lumStdDev = std::sqrt(variance);
    }
    // Rewards coverage, brightness, and structure (spread) together so a
    // bright, structured flame outranks a flat, faint haze at equal
    // coveredFraction - the discrimination best-of-N fallback (MainWindow's
    // random batch retry loop) needs when every attempt fails the user's
    // minCoverageFraction bar and it must pick the least-bad one. Bounded to
    // [0, coveredFraction] since the two terms are clamped to [0,1].
    const double lumTerm = std::clamp(out.meanForegroundLum / kLumReference, 0.0, 1.0);
    const double contrastTerm = std::clamp(out.lumStdDev / kContrastReference, 0.0, 1.0);
    out.score = out.coveredFraction * (0.60 + 0.25 * lumTerm + 0.15 * contrastTerm);
    return out;
}

} // namespace apo
