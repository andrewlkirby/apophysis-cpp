#include "VarCross.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarCross::calc() {
    // The original computes Abs(...) and then has a redundant `if r < 0
    // then r := r * -1.0` immediately after - dead code (Abs already
    // guarantees non-negative), not reproduced here since it has zero
    // behavioral effect either way.
    double r = std::fabs((*tx - *ty) * (*tx + *ty) + 1e-6);
    r = vvar / r;

    *px += *tx * r;
    *py += *ty * r;
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarCross>();
}

} // namespace apo
