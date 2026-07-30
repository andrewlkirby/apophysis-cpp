#include "VarBlade.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

void VarBlade::calc() {
    const double rad = std::sqrt(*tx * *tx + *ty * *ty);
    const double r = rng->uniform01() * vvar * rad;
    const double sinr = std::sin(r), cosr = std::cos(r);

    *px += vvar * *tx * (cosr + sinr);
    *py += vvar * *tx * (cosr - sinr);
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarBlade>();
}

} // namespace apo
