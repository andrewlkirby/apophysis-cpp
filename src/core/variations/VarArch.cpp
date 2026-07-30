#include "VarArch.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void VarArch::calc() {
    const double ang = rng->uniform01() * vvar * kPi;
    const double sinr = std::sin(ang), cosr = std::cos(ang);

    *px += vvar * sinr;
    *py += vvar * (sinr * sinr) / cosr;
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarArch>();
}

} // namespace apo
