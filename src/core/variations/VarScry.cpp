#include "VarScry.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarScry::prepare() { v_ = (vvar == 0) ? 1.0 / 1e-6 : 1.0 / vvar; }

void VarScry::calc() {
    const double t = (*tx) * (*tx) + (*ty) * (*ty);
    const double r = 1.0 / (std::sqrt(t) * (t + v_));

    *px += *tx * r;
    *py += *ty * r;
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarScry>();
}

} // namespace apo
