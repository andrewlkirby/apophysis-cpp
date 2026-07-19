// Tests for Phase 6's Editor-window triangle math (src/core/edit/
// TriangleGeometry.h) - the coefficient<->vertex conversion this whole
// widget hinges on, plus the rotate/scale/hit-testing helpers.

#include <cmath>
#include <vector>

#include "TestHelpers.h"
#include "core/edit/TriangleGeometry.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

// Not <cmath>'s M_PI: MSVC only defines that behind _USE_MATH_DEFINES.
constexpr double kPi = 3.14159265358979323846;

void testIdentityCoefsProduceUnitTriangle() {
    // a=1,d=0,b=0,e=1,c=0,f=0 (identity) -> O=(0,0), X=(1,0), Y=(0,1),
    // matching the O/X/Y = "origin / where (1,0) goes / where (0,1) goes"
    // semantics exactly.
    const std::array<std::array<double, 2>, 3> identity{{{1, 0}, {0, 1}, {0, 0}}};
    const apo::Triangle t = apo::triangleFromCoefs(identity);
    check(approxEqual(t.o.x, 0) && approxEqual(t.o.y, 0), "identity: O is at the origin");
    check(approxEqual(t.x.x, 1) && approxEqual(t.x.y, 0), "identity: X is at (1,0)");
    check(approxEqual(t.y.x, 0) && approxEqual(t.y.y, 1), "identity: Y is at (0,1)");
}

void testForwardInverseRoundTrip() {
    // A handful of arbitrary, non-degenerate affine coefficient sets -
    // triangleFromCoefs then coefsFromTriangle should recover the exact
    // originals (verified algebraically in TriangleGeometry.h's own
    // comment; this is the executable form of that proof).
    const std::vector<std::array<std::array<double, 2>, 3>> cases = {
        {{{1, 0}, {0, 1}, {0, 0}}},
        {{{0.5, 0.3}, {-0.3, 0.5}, {0.2, -0.4}}},
        {{{2.0, 0.0}, {0.0, -1.5}, {-3.0, 7.0}}},
        {{{0.1, 0.1}, {0.1, 0.1}, {0.0, 0.0}}}, // degenerate (zero area) but still a valid round-trip input
    };

    bool allOk = true;
    for (const auto& coefs : cases) {
        const apo::Triangle t = apo::triangleFromCoefs(coefs);
        const auto recovered = apo::coefsFromTriangle(t);
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 2; ++col) {
                if (!approxEqual(coefs[row][col], recovered[row][col], 1e-9)) allOk = false;
            }
        }
    }
    check(allOk, "coefsFromTriangle(triangleFromCoefs(c)) recovers c exactly, across several coefficient sets");
}

void testCentroid() {
    const apo::Triangle t{{0, 0}, {3, 0}, {0, 3}};
    const apo::Point2 c = apo::centroid(t);
    check(approxEqual(c.x, 1.0) && approxEqual(c.y, 1.0), "centroid of a simple right triangle");
}

void testRotateAroundPivot90Degrees() {
    const apo::Point2 p{1, 0};
    const apo::Point2 pivot{0, 0};
    const apo::Point2 rotated = apo::rotateAroundPivot(p, pivot, kPi / 2);
    check(approxEqual(rotated.x, 0, 1e-9) && approxEqual(rotated.y, 1, 1e-9),
          "rotating (1,0) by 90 degrees around the origin gives (0,1)");
}

void testScaleAroundPivot() {
    const apo::Point2 p{3, 3};
    const apo::Point2 pivot{1, 1};
    const apo::Point2 scaled = apo::scaleAroundPivot(p, pivot, 2.0);
    check(approxEqual(scaled.x, 5) && approxEqual(scaled.y, 5), "scaling (3,3) by 2x around (1,1) gives (5,5)");

    const apo::Point2 back = apo::scaleAroundPivot(scaled, pivot, 1.0);
    check(approxEqual(back.x, scaled.x) && approxEqual(back.y, scaled.y), "scaling by factor 1.0 is a no-op");
}

void testAngleBetween() {
    const apo::Point2 pivot{0, 0};
    const double a = apo::angleBetween(pivot, {1, 0}, {0, 1});
    check(approxEqual(a, kPi / 2, 1e-9), "angleBetween((1,0), (0,1)) around the origin is +90 degrees");
}

void testScaleFactorBetween() {
    const apo::Point2 pivot{0, 0};
    // Dragging exactly along the same ray, twice as far -> factor 2.
    const double t1 = apo::scaleFactorBetween(pivot, {2, 0}, {4, 0});
    check(approxEqual(t1, 2.0, 1e-9), "scaleFactorBetween along the same ray gives the plain distance ratio");

    // Dragging perpendicular to the original ray -> projection is ~0.
    const double t2 = apo::scaleFactorBetween(pivot, {2, 0}, {2, 5});
    check(approxEqual(t2, 1.0, 1e-9),
          "scaleFactorBetween for a purely perpendicular drag keeps the original-axis component unchanged (t=1)");
}

void testDistanceToSegment() {
    // Perpendicular distance from a point directly "above" the segment's
    // midpoint.
    const double d1 = apo::distanceToSegment({5, 3}, {0, 0}, {10, 0});
    check(approxEqual(d1, 3.0, 1e-9), "distanceToSegment: perpendicular distance to a horizontal segment");

    // A point beyond the segment's end clamps to the endpoint distance,
    // not the infinite line's distance.
    const double d2 = apo::distanceToSegment({15, 0}, {0, 0}, {10, 0});
    check(approxEqual(d2, 5.0, 1e-9), "distanceToSegment clamps to the nearest endpoint past the segment's end");
}

void testPointInTriangle() {
    const apo::Triangle t{{0, 0}, {4, 0}, {0, 4}};
    check(apo::pointInTriangle({1, 1}, t), "a point well inside the triangle is detected as inside");
    check(!apo::pointInTriangle({10, 10}, t), "a point far outside the triangle is detected as outside");
    check(apo::pointInTriangle({0, 0}, t), "a vertex itself counts as inside (boundary-inclusive)");
    check(!apo::pointInTriangle({-1, -1}, t), "a point just outside the origin corner is detected as outside");
}

} // namespace

int main() {
    testIdentityCoefsProduceUnitTriangle();
    testForwardInverseRoundTrip();
    testCentroid();
    testRotateAroundPivot90Degrees();
    testScaleAroundPivot();
    testAngleBetween();
    testScaleFactorBetween();
    testDistanceToSegment();
    testPointInTriangle();
    return apo_test::reportAndExit();
}
