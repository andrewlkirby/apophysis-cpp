// Tests for Renderer::samplePoints (src/core/render/Renderer.h) and
// autoFrameFlame (src/core/render/AutoFrame.h) - the camera-framing
// primitive needed for generating standalone random flames (unlike
// Mutate's mutants, which always inherit their base flame's existing
// camera - see AutoFrame.h's header comment).

#include <cmath>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/render/AutoFrame.h"
#include "core/render/Renderer.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::unique_ptr<apo::Flame> makeSierpinskiFlame(double offsetX = 0.0, double offsetY = 0.0) {
    auto flame = std::make_unique<apo::Flame>();
    flame->width = 100;
    flame->height = 100;

    const double corners[3][2] = {{-0.5, -0.5}, {0.5, -0.5}, {0.0, 0.5}};
    for (int i = 0; i < 3; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.color = i / 2.0;
        xf.c[0] = {0.5, 0.0};
        xf.c[1] = {0.0, 0.5};
        xf.c[2] = {corners[i][0] + offsetX, corners[i][1] + offsetY};
    }
    return flame;
}

void testSamplePointsReturnsRequestedCountForAWellBehavedFlame() {
    auto flame = makeSierpinskiFlame();
    const auto points = apo::Renderer::samplePoints(*flame, /*seed=*/1, /*count=*/5000);
    check(points.size() == 5000, "samplePoints returns exactly `count` points for a well-behaved, bounded flame");
}

void testSamplePointsIsEmptyForADegenerateFlame() {
    apo::Flame flame; // fresh, every xform has density=0
    const auto points = apo::Renderer::samplePoints(flame, /*seed=*/1, /*count=*/1000);
    check(points.empty(), "samplePoints returns nothing for a flame with no active xforms, instead of crashing");
}

void testSamplePointsIsDeterministicGivenSeed() {
    auto flame = makeSierpinskiFlame();
    const auto a = apo::Renderer::samplePoints(*flame, /*seed=*/42, 500);
    const auto b = apo::Renderer::samplePoints(*flame, /*seed=*/42, 500);
    check(a == b, "samplePoints is bit-reproducible for the same flame and seed");

    const auto c = apo::Renderer::samplePoints(*flame, /*seed=*/43, 500);
    check(a != c, "samplePoints produces different output for a different seed");
}

void testAutoFrameFlameCentersOnAnOffsetAttractor() {
    auto flame = makeSierpinskiFlame(/*offsetX=*/50.0, /*offsetY=*/30.0);
    flame->center = {0, 0};
    flame->pixelsPerUnit = 50;

    const bool framed = apo::autoFrameFlame(*flame, /*seed=*/7);
    check(framed, "a real, non-degenerate attractor reports successful framing");

    // Each xform is a 0.5-scale contraction x' = 0.5x + c - shifting every
    // map's translation c by a constant d moves its (and so the whole
    // IFS attractor's) fixed point by d/(1-0.5) = 2d, not by d itself
    // (verified directly: solving x = 0.5x + c + d gives x = 2(c+d), vs.
    // x = 2c unshifted - a factor of 2, not 1). So a (50, 30) shift in the
    // coefficients should land the computed center near (100, 60).
    check(std::abs(flame->center[0] - 100.0) < 3.0, "autoFrameFlame's center.x lands close to the shifted attractor");
    check(std::abs(flame->center[1] - 60.0) < 3.0, "autoFrameFlame's center.y lands close to the shifted attractor");
    check(flame->pixelsPerUnit > 10.0, "a real (non-degenerate) attractor gets a proportional, non-fallback pixelsPerUnit");
}

void testAutoFrameFlameRejectsATightlyClusteredAttractor() {
    // A single strongly-contracting linear xform - its chaos-game
    // iteration converges toward one fixed point (a "cluster" attractor
    // with ~zero spread). CalcBoundbox's original near-zero-area fallback
    // would frame this at a default pixelsPerUnit=10, but a collapsed-to-
    // one-point attractor is visually indistinguishable from blank at any
    // zoom, so this port now treats it as degenerate instead (see
    // autoFrameFlame's own doc comment).
    auto flame = std::make_unique<apo::Flame>();
    flame->width = 100;
    flame->height = 100;
    flame->center = {1.0, 2.0};
    flame->pixelsPerUnit = 42.0;
    apo::XForm& xf = *flame->xform[0];
    xf.clear();
    xf.density = 1.0;
    xf.c[0] = {0.1, 0.0};
    xf.c[1] = {0.0, 0.1};
    xf.c[2] = {10.0, -20.0}; // fixed point at c/(1-0.1) = (11.11, -22.22)

    const bool framed = apo::autoFrameFlame(*flame, /*seed=*/3);
    check(!framed, "a tightly-clustered (near-zero-spread) attractor reports failed framing, not a fallback frame");
    check(approxEqual(flame->center[0], 1.0) && approxEqual(flame->center[1], 2.0),
          "...and its center is left untouched");
    check(approxEqual(flame->pixelsPerUnit, 42.0), "...and its pixelsPerUnit is left untouched");
}

void testAutoFrameFlameRejectsAnImplausiblyLargeAttractor() {
    // A single strongly-expanding linear xform - its chaos-game iteration
    // diverges toward increasingly large coordinates without ever hitting
    // non-finite values, so it passes the finite-sample-count check but
    // still trips the other CalcBoundbox-derived fallback: an implausibly
    // large (>1000-unit) trimmed extent. CalcBoundbox's original fallback
    // forced center=(0,0) here, which usually isn't anywhere near the
    // actual attractor - this port now treats it as degenerate instead
    // (see autoFrameFlame's own doc comment).
    auto flame = std::make_unique<apo::Flame>();
    flame->width = 100;
    flame->height = 100;
    flame->center = {1.0, 2.0};
    flame->pixelsPerUnit = 42.0;
    apo::XForm& xf = *flame->xform[0];
    xf.clear();
    xf.density = 1.0;
    xf.c[0] = {1.5, 0.0};
    xf.c[1] = {0.0, 1.5};
    xf.c[2] = {500.0, 500.0};

    const bool framed = apo::autoFrameFlame(*flame, /*seed=*/5);
    check(!framed, "an implausibly large (>1000-unit) attractor extent reports failed framing, not a fallback frame");
    check(approxEqual(flame->center[0], 1.0) && approxEqual(flame->center[1], 2.0),
          "...and its center is left untouched");
    check(approxEqual(flame->pixelsPerUnit, 42.0), "...and its pixelsPerUnit is left untouched");
}

void testAutoFrameFlameHonorsAConfigurableMinValidSampleCount() {
    // A flame whose single xform has transOpacity=0 (excluded by
    // samplePoints - see its own IterateXY-matching comment) produces zero
    // sampled points, same as the "no active xforms" case below - cheap way
    // to get a small-but-nonzero-ish boundary to probe minValidSamples
    // against without relying on real numerical divergence.
    auto flame = makeSierpinskiFlame();

    const bool framedWithDefault = apo::autoFrameFlame(*flame, /*seed=*/9);
    check(framedWithDefault, "a well-behaved attractor still frames successfully at the default minValidSamples");

    auto flame2 = makeSierpinskiFlame();
    const bool framedWithImpossibleMinimum = apo::autoFrameFlame(*flame2, /*seed=*/9, /*minValidSamples=*/1000000);
    check(!framedWithImpossibleMinimum,
          "raising minValidSamples above what any run could produce forces failed framing, confirming the "
          "parameter is actually threaded through");
}

void testAutoFrameFlameLeavesDegenerateFlameUntouched() {
    apo::Flame flame;
    flame.width = 100;
    flame.height = 100;
    flame.center = {1.0, 2.0};
    flame.pixelsPerUnit = 42.0;

    const bool framed = apo::autoFrameFlame(flame, /*seed=*/1);

    check(!framed, "a flame with no active xforms (too few sampled points) reports failed framing");
    check(approxEqual(flame.center[0], 1.0) && approxEqual(flame.center[1], 2.0),
          "a flame with no active xforms is left with its center untouched (nothing to sample)");
    check(approxEqual(flame.pixelsPerUnit, 42.0), "...and its pixelsPerUnit untouched too");
}

} // namespace

int main() {
    testSamplePointsReturnsRequestedCountForAWellBehavedFlame();
    testSamplePointsIsEmptyForADegenerateFlame();
    testSamplePointsIsDeterministicGivenSeed();
    testAutoFrameFlameCentersOnAnOffsetAttractor();
    testAutoFrameFlameRejectsATightlyClusteredAttractor();
    testAutoFrameFlameRejectsAnImplausiblyLargeAttractor();
    testAutoFrameFlameHonorsAConfigurableMinValidSampleCount();
    testAutoFrameFlameLeavesDegenerateFlameUntouched();

    return apo_test::reportAndExit();
}
