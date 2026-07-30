#include "VarSecant2.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarSecant2::calc() {
    const double rad = vvar * std::sqrt(*tx * *tx + *ty * *ty);
    const double cr = std::cos(rad);
    const double icr = 1.0 / cr;

    *px += vvar * *tx;
    *py += (cr < 0.0) ? vvar * (icr + 1.0) : vvar * (icr - 1.0);
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarSecant2>();
}

} // namespace apo
