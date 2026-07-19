#include "VarHemisphere.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarHemisphere::calc() {
    const double t = vvar / std::sqrt((*tx) * (*tx) + (*ty) * (*ty) + 1);

    *px += *tx * t;
    *py += *ty * t;
    // Matches the Pascal exactly: `FPz^ := FPz^ + t;` - not `vvar * t` or
    // `t * FTz^` (both of which would be the more "expected" shapes given
    // the Px/Py lines) - this variation just adds the raw scale factor t to
    // Pz regardless of Tz, verified by reading the source directly.
    *pz += t;
}

namespace {
const bool kRegistered = registerVariation<VarHemisphere>();
}

} // namespace apo
