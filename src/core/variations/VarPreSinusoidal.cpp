#include "VarPreSinusoidal.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarPreSinusoidal::calc() {
    *tx = vvar * std::sin(*tx);
    *ty = vvar * std::sin(*ty);
    *tz = vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarPreSinusoidal>();
}

} // namespace apo
