#pragma once

#include <array>

namespace apo {

// A rational cubic Bezier curve - one Adjust-dialog Curves-tab tone-curve
// channel (see Flame.h's `curves` field) or, more generally, any 4-point
// weighted cubic Bezier. Matches Core/Bezier.pas's BezierPoints/
// BezierWeights exactly (points[i] = (x,y), weights[i] - both indexed
// 0..3).
//
// P0 is always (0,0) and P3 is always (1,1) by convention for a tone
// curve (it always maps black to black and white to white) - the Curves
// dialog only ever drags points[1]/points[2] and their weights, but all 4
// points/weights are stored and round-tripped through .flame files, since
// a hand-authored or third-party-tool-written file could in principle set
// them to anything.
struct BezierCurve {
    std::array<std::array<double, 2>, 4> points = {{{0.0, 0.0}, {0.0, 0.0}, {1.0, 1.0}, {1.0, 1.0}}};
    std::array<double, 4> weights = {1.0, 1.0, 1.0, 1.0};

    bool operator==(const BezierCurve& other) const = default;
};

// Evaluates the curve's full parametric point (x(t), y(t)) at t in
// [0,1] - matches Bezier.pas's BezierSolve. Used by the Curves dialog to
// actually draw the curve. Returns (0,0) for a degenerate curve (e.g. all
// weights <= 0) instead of leaving the result undefined the way the
// original's early-return-without-setting-`solution` does - a real
// .flame file with corrupted/adversarial weight values shouldn't be able
// to produce an uninitialized read.
std::array<double, 2> solveBezierCurve(const BezierCurve& curve, double t);

// Evaluates just the curve's y coordinate at parameter t - matches
// Bezier.pas's BezierFunc, the entry point ImageMaker.pas's tone-mapping
// actually uses (see Renderer.cpp's toneMap()): it passes t = i/256
// directly as if it were the curve's *x* coordinate, which is only a
// reasonable approximation because P0=(0,0)/P3=(1,1) constrain the curve
// to be visually monotonic in practice - the original itself relies on
// this same simplification (it never inverts the parametric curve to
// solve for a true x-indexed y=f(x) lookup), so this port matches that
// behavior rather than "improving" it into a different, incompatible
// curve shape.
double evalBezierCurve(const BezierCurve& curve, double t);

} // namespace apo
