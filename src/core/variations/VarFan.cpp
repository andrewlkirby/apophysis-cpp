#include "VarFan.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kEps = 1e-10;
constexpr double kPi = 3.14159265358979323846;
}

void VarFan::prepare() {
    dx_ = kPi * (e * e + kEps);
    dy_ = f;
    dx2_ = dx_ / 2.0;
}

void VarFan::calc() {
    double ang = std::atan2(*tx, *ty);
    const double rad = vvar * std::sqrt(*tx * *tx + *ty * *ty);

    ang += (std::fmod(ang + dy_, dx_) > dx2_) ? -dx2_ : dx2_;

    const double sa = std::sin(ang), ca = std::cos(ang);

    *px += rad * ca;
    *py += rad * sa;
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarFan>();
}

} // namespace apo
