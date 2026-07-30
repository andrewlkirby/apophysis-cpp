#include "VarPopcorn.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarPopcorn::calc() {
    const double dxv = std::tan(3.0 * *ty);
    const double dyv = std::tan(3.0 * *tx);

    const double nx = *tx + e * std::sin(dxv);
    const double ny = *ty + f * std::sin(dyv);

    *px += vvar * nx;
    *py += vvar * ny;
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarPopcorn>();
}

} // namespace apo
