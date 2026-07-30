#include "VarHeart.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarHeart::calc() {
    const double rad = std::sqrt(*tx * *tx + *ty * *ty);
    const double angle = std::atan2(*tx, *ty);
    const double heartAngle = rad * angle;
    const double r = vvar * rad;

    *px += r * std::sin(heartAngle);
    *py += -r * std::cos(heartAngle);
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarHeart>();
}

} // namespace apo
