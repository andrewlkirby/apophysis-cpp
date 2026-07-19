#include "Bezier.h"

#include <cmath>

namespace apo {

std::array<double, 2> solveBezierCurve(const BezierCurve& curve, double t) {
    const double s = 1 - t;
    const double s2 = s * s;
    const double s3 = s2 * s;
    const double t2 = t * t;
    const double t3 = t2 * t;

    const auto& p = curve.points;
    const auto& w = curve.weights;

    const double nomX =
        w[0] * s3 * p[0][0] + w[1] * s2 * 3 * t * p[1][0] + w[2] * s * 3 * t2 * p[2][0] + w[3] * t3 * p[3][0];
    const double nomY =
        w[0] * s3 * p[0][1] + w[1] * s2 * 3 * t * p[1][1] + w[2] * s * 3 * t2 * p[2][1] + w[3] * t3 * p[3][1];
    const double denom = w[0] * s3 + w[1] * s2 * 3 * t + w[2] * s * 3 * t2 + w[3] * t3;

    if (!std::isfinite(nomX) || !std::isfinite(nomY) || !std::isfinite(denom) || denom == 0) {
        return {0.0, 0.0};
    }
    return {nomX / denom, nomY / denom};
}

double evalBezierCurve(const BezierCurve& curve, double t) {
    return solveBezierCurve(curve, t)[1];
}

} // namespace apo
