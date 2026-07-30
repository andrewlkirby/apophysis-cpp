#include "VarCosine.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void VarCosine::calc() {
    const double ang = *tx * kPi;
    const double sa = std::sin(ang), ca = std::cos(ang);

    const double nx = ca * std::cosh(*ty);
    const double ny = -sa * std::sinh(*ty);

    *px += vvar * nx;
    *py += vvar * ny;
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarCosine>();
}

} // namespace apo
