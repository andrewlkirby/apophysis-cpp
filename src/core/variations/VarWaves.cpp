#include "VarWaves.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kEps = 1e-10;
}

void VarWaves::prepare() {
    dx2_ = 1.0 / (e * e + kEps);
    dy2_ = 1.0 / (f * f + kEps);
}

void VarWaves::calc() {
    const double nx = *tx + c * std::sin(*ty * dx2_);
    const double ny = *ty + d * std::sin(*tx * dy2_);

    *px += vvar * nx;
    *py += vvar * ny;
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarWaves>();
}

} // namespace apo
