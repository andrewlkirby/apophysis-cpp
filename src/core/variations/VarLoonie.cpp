#include "VarLoonie.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarLoonie::prepare() { sqrVar_ = vvar * vvar; }

void VarLoonie::calc() {
    const double r2 = (*tx) * (*tx) + (*ty) * (*ty);

    if (r2 < sqrVar_ && r2 != 0) {
        const double r = vvar * std::sqrt(sqrVar_ / r2 - 1.0);
        *px += r * *tx;
        *py += r * *ty;
    } else {
        *px += vvar * *tx;
        *py += vvar * *ty;
    }

    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarLoonie>();
}

} // namespace apo
