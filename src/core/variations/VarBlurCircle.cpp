#include "VarBlurCircle.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

void VarBlurCircle::prepare() {
    vvar4Pi_ = vvar * 4.0 / kPi;
    pi4_ = kPi / 4.0;
}

void VarBlurCircle::calc() {
    const double x = 2.0 * rng->uniform01() - 1.0;
    const double y = 2.0 * rng->uniform01() - 1.0;

    const double absx = std::fabs(x);
    const double absy = std::fabs(y);

    double side, perimeter;
    if (absx >= absy) {
        perimeter = (x >= absy) ? (absx + y) : (5.0 * absx - y);
        side = absx;
    } else {
        perimeter = (y >= absx) ? (3.0 * absy - x) : (7.0 * absy + x);
        side = absy;
    }

    const double r = vvar * side;
    const double angle = pi4_ * perimeter / side - pi4_;

    *px += r * std::cos(angle);
    *py += r * std::sin(angle);
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarBlurCircle>();
}

} // namespace apo
