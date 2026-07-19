// Tests for the Bezier curve math (src/core/Bezier.h) backing the Curves
// dialog's tone curves - verified directly against Core/Bezier.pas.

#include "TestHelpers.h"
#include "core/Bezier.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

void testDefaultCurveEndpointsAreFixed() {
    apo::BezierCurve curve;
    const auto p0 = apo::solveBezierCurve(curve, 0.0);
    const auto p1 = apo::solveBezierCurve(curve, 1.0);
    check(approxEqual(p0[0], 0.0) && approxEqual(p0[1], 0.0), "t=0 always lands exactly on P0=(0,0)");
    check(approxEqual(p1[0], 1.0) && approxEqual(p1[1], 1.0), "t=1 always lands exactly on P3=(1,1)");
}

void testDefaultCurveIsTheDiagonalLine() {
    // P0=(0,0), P1=(0,0), P2=(1,1), P3=(1,1), all weights 1 - even though
    // x(t) and y(t) individually aren't equal to t (they're both
    // t^2*(3-2t), a smooth non-uniform reparametrization), x(t) and y(t)
    // are always equal to *each other*, so the traced curve is still
    // exactly the line y=x - matches Renderer.cpp's toneMap() comment on
    // why the original's own "is this curve just the default?" check
    // exists (checking point positions, not evaluating the table, is what
    // avoids a numerically-non-identity lookup for the default case).
    apo::BezierCurve curve;
    for (double t = 0.0; t <= 1.0; t += 0.1) {
        const auto p = apo::solveBezierCurve(curve, t);
        check(approxEqual(p[0], p[1], 1e-9), "the default curve's x(t) and y(t) are equal at every t (a straight diagonal)");
    }
}

void testEvalBezierCurveReturnsOnlyY() {
    apo::BezierCurve curve;
    curve.points[1] = {0.0, 0.5}; // pull the curve upward early on
    curve.points[2] = {1.0, 0.5};
    const auto full = apo::solveBezierCurve(curve, 0.3);
    const double y = apo::evalBezierCurve(curve, 0.3);
    check(approxEqual(full[1], y), "evalBezierCurve returns exactly solveBezierCurve's y component");
}

void testMovingControlPointsBendsTheCurveUpward() {
    // A classic "brighten the midtones" curve: P1 pulled up toward
    // (0, 0.5), pulling the whole curve above the diagonal for t in (0,1).
    apo::BezierCurve curve;
    curve.points[1] = {0.0, 0.75};
    curve.points[2] = {0.25, 1.0};
    const double y = apo::evalBezierCurve(curve, 0.5);
    check(y > 0.5, "pulling P1/P2 upward bends the curve's midpoint above the y=x diagonal");
}

void testDegenerateWeightsDoNotProduceGarbage() {
    apo::BezierCurve curve;
    curve.weights = {0.0, 0.0, 0.0, 0.0}; // denom is identically 0 for every t
    const auto p = apo::solveBezierCurve(curve, 0.5);
    check(approxEqual(p[0], 0.0) && approxEqual(p[1], 0.0),
          "all-zero weights (a degenerate/adversarial curve) resolves to a defined (0,0), not an uninitialized read");
}

void testCurveEqualityOperator() {
    apo::BezierCurve a, b;
    check(a == b, "two default-constructed curves compare equal");
    b.points[1] = {0.1, 0.2};
    check(!(a == b), "changing a control point makes curves compare unequal");
}

} // namespace

int main() {
    testDefaultCurveEndpointsAreFixed();
    testDefaultCurveIsTheDiagonalLine();
    testEvalBezierCurveReturnsOnlyY();
    testMovingControlPointsBendsTheCurveUpward();
    testDegenerateWeightsDoNotProduceGarbage();
    testCurveEqualityOperator();

    return apo_test::reportAndExit();
}
