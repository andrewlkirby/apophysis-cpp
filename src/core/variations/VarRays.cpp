#include "VarRays.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kEps = 1e-10;
constexpr double kPi = 3.14159265358979323846;
}

void VarRays::calc() {
    const double ang = vvar * rng->uniform01() * kPi;
    const double r = vvar / (*tx * *tx + *ty * *ty + kEps);
    const double tanr = vvar * std::tan(ang) * r;

    *px += tanr * std::cos(*tx);
    *py += tanr * std::sin(*ty);
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarRays>();
}

} // namespace apo
