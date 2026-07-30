#include "VarPower.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarPower::calc() {
    const double rad = std::sqrt(*tx * *tx + *ty * *ty);
    const double sina = *tx / rad;
    const double cosa = *ty / rad;
    const double r = vvar * std::pow(rad, sina);

    *px += r * cosa;
    *py += r * sina;
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarPower>();
}

} // namespace apo
