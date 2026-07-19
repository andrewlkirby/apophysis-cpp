#include "VarPreSpherical.h"

#include "../VariationRegistration.h"

namespace apo {

void VarPreSpherical::calc() {
    // Pascal literal is `10e-6` (= 1e-5), not the usual 1e-6/1E-300 epsilon
    // seen elsewhere - preserved exactly as written, not normalized.
    const double r = vvar / ((*tx) * (*tx) + (*ty) * (*ty) + 10e-6);
    *tx = *tx * r;
    *ty = *ty * r;
    *tz = vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarPreSpherical>();
}

} // namespace apo
