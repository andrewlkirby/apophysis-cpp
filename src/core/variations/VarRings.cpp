#include "VarRings.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kEps = 1e-10;
}

void VarRings::prepare() { dx_ = e * e + kEps; }

void VarRings::calc() {
    const double rad = std::sqrt(*tx * *tx + *ty * *ty);
    const double sina = *tx / rad;
    const double cosa = *ty / rad;

    const double r = vvar * (std::fmod(rad + dx_, 2.0 * dx_) - dx_ + rad * (1.0 - dx_));

    *px += r * cosa;
    *py += r * sina;
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarRings>();
}

} // namespace apo
