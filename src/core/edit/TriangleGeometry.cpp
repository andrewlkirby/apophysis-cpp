#include "TriangleGeometry.h"

#include <algorithm>
#include <cmath>

namespace apo {

Triangle triangleFromCoefs(const std::array<std::array<double, 2>, 3>& coefs) {
    const double a = coefs[0][0], d = coefs[0][1];
    const double b = coefs[1][0], e = coefs[1][1];
    const double c = coefs[2][0], f = coefs[2][1];

    Triangle t;
    t.o = {c, -f};
    t.x = {a + c, -(d + f)};
    t.y = {c - b, e - f};
    return t;
}

std::array<std::array<double, 2>, 3> coefsFromTriangle(const Triangle& t) {
    const double a = t.x.x - t.o.x;
    const double b = t.o.x - t.y.x;
    const double c = t.o.x;
    const double d = t.o.y - t.x.y;
    const double e = t.y.y - t.o.y;
    const double f = -t.o.y;
    return {{{a, d}, {b, e}, {c, f}}};
}

Point2 centroid(const Triangle& t) {
    return {(t.o.x + t.x.x + t.y.x) / 3.0, (t.o.y + t.x.y + t.y.y) / 3.0};
}

Point2 rotateAroundPivot(Point2 p, Point2 pivot, double angleRadians) {
    const double s = std::sin(angleRadians);
    const double c = std::cos(angleRadians);
    const double dx = p.x - pivot.x;
    const double dy = p.y - pivot.y;
    return {pivot.x + dx * c - dy * s, pivot.y + dx * s + dy * c};
}

Point2 scaleAroundPivot(Point2 p, Point2 pivot, double factor) {
    return {pivot.x + (p.x - pivot.x) * factor, pivot.y + (p.y - pivot.y) * factor};
}

double angleBetween(Point2 pivot, Point2 from, Point2 to) {
    const double angleFrom = std::atan2(from.y - pivot.y, from.x - pivot.x);
    const double angleTo = std::atan2(to.y - pivot.y, to.x - pivot.x);
    return angleTo - angleFrom;
}

double scaleFactorBetween(Point2 pivot, Point2 from, Point2 to) {
    const double oldx = from.x - pivot.x;
    const double oldy = from.y - pivot.y;
    const double newx = to.x - pivot.x;
    const double newy = to.y - pivot.y;
    const double oldDistSq = oldx * oldx + oldy * oldy;
    if (oldDistSq == 0) return 1.0; // degenerate: mouse-down was exactly on the pivot
    return (oldx * newx + oldy * newy) / oldDistSq;
}

double distance(Point2 a, Point2 b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

double distanceToSegment(Point2 p, Point2 a, Point2 b) {
    const double abx = b.x - a.x;
    const double aby = b.y - a.y;
    const double lengthSq = abx * abx + aby * aby;
    if (lengthSq == 0) return distance(p, a); // a == b: degenerate segment

    double t = ((p.x - a.x) * abx + (p.y - a.y) * aby) / lengthSq;
    t = std::max(0.0, std::min(1.0, t));
    return distance(p, {a.x + t * abx, a.y + t * aby});
}

bool pointInTriangle(Point2 p, const Triangle& t) {
    // Standard sign-of-cross-product test: `p` is inside iff it's on the
    // same side of all three directed edges.
    auto sign = [](Point2 p1, Point2 p2, Point2 p3) {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };

    const double d1 = sign(p, t.o, t.x);
    const double d2 = sign(p, t.x, t.y);
    const double d3 = sign(p, t.y, t.o);

    const bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

} // namespace apo
