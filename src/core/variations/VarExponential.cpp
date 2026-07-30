#include "VarExponential.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void VarExponential::calc() {
    const double dx = vvar * std::exp(*tx - 1.0);
    const double dy = kPi * *ty;

    *px += dx * std::cos(dy);
    *py += dx * std::sin(dy);
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarExponential>();
}

} // namespace apo
