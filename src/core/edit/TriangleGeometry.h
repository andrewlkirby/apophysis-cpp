#pragma once

#include <array>

namespace apo {

// Pure math for the Editor window's transform-triangle widget
// (src/ui/TriangleCanvas), kept Qt-free and unit-testable on its own
// (see tests/triangle_geometry_test.cpp) - deliberately separated from the
// Qt widget code, which is thin glue over this plus paint calls.
//
// This is a from-scratch design, not a port: the migration plan itself
// flags Editor.pas's TriangleView as "the single biggest UI risk...
// bespoke hit-testing math [needing] a from-scratch rewrite, not a
// mechanical port." What *is* preserved from the original (verified
// directly against Flame/ControlPoint.pas's GetTriangle/GetFromTriangles/
// TrianglesFromCP) is the vertex semantics below, since that's what makes
// a flame file's xforms show up as recognizable triangles at all - get
// that wrong and every flame looks broken. The drag-interaction model
// (hit-testing regions, modifier keys, pivot handling) is intentionally
// simplified from the original's more elaborate edge/widget-hit-zone
// system - see TriangleCanvas.h for what was kept vs. simplified and why.
struct Point2 {
    double x = 0, y = 0;
};

// The three vertices of one xform's transform triangle, named to match
// the original UI's own vertex labels ("X"/"O"/"Y" - not to be confused
// with Point2's x/y fields).
struct Triangle {
    Point2 o, x, y;
};

// Coefficients -> triangle, matching TrianglesFromCP exactly (verified
// directly against ControlPoint.pas): O = where the affine transform
// sends the origin, X = where it sends (1,0), Y = where it sends (0,1) -
// i.e. the vertices are exactly the transform's own basis image, which is
// what makes dragging them an intuitive way to edit the transform.
// `coefs` uses XForm.h's own [row][col] layout: coefs[0]=(a,d),
// coefs[1]=(b,e), coefs[2]=(c,f).
Triangle triangleFromCoefs(const std::array<std::array<double, 2>, 3>& coefs);

// Triangle -> coefficients, the exact closed-form inverse of
// triangleFromCoefs (algebraically derived, cross-checked against
// GetFromTriangles's general 3x3 linear solve - they agree). No
// iteration, no degenerate cases beyond the triangle being degenerate
// itself (zero area - which is also singular for the original's solve,
// not a new limitation).
std::array<std::array<double, 2>, 3> coefsFromTriangle(const Triangle& t);

Point2 centroid(const Triangle& t);

// Rotates `p` by `angleRadians` around `pivot`. Used for both whole-
// triangle drags (apply to all 3 vertices) and single-vertex drags
// (apply to just the grabbed one) - the original does the same, per the
// research behind this port: "Rotate/Scale math for a single corner is
// the same pivot-relative formula...applied to 3 points [or] one point".
Point2 rotateAroundPivot(Point2 p, Point2 pivot, double angleRadians);

// Scales `p` by `factor` around `pivot` (factor 1.0 = no change).
Point2 scaleAroundPivot(Point2 p, Point2 pivot, double factor);

// The incremental rotation angle (radians) between two points as seen
// from `pivot` - e.g. angle(mouseCurrent) - angle(mouseDown), both
// relative to pivot. Matches TriangleView's rotate-drag angle formula.
double angleBetween(Point2 pivot, Point2 from, Point2 to);

// The scale factor implied by dragging from `from` to `to`, both
// relative to `pivot`: the scalar projection of (to-pivot) onto
// (from-pivot), normalized by |from-pivot|^2 - so a drag exactly along
// the original ray gives the intuitive distance ratio, and off-axis drags
// degrade gracefully rather than requiring a precisely radial gesture.
// Matches TriangleView's scale-drag formula exactly (verified directly
// against Editor.pas).
double scaleFactorBetween(Point2 pivot, Point2 from, Point2 to);

// Euclidean distance between two points.
double distance(Point2 a, Point2 b);

// Perpendicular distance from `p` to the line segment [a,b] (not the
// infinite line - clamps to the nearest endpoint past the segment's
// ends).
double distanceToSegment(Point2 p, Point2 a, Point2 b);

// True if `p` lies inside (or on the boundary of) the triangle - used to
// distinguish a whole-triangle-body drag from a click on empty canvas.
bool pointInTriangle(Point2 p, const Triangle& t);

} // namespace apo
