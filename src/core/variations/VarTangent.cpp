#include "VarTangent.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarTangent::calc() {
    *px += vvar * std::sin(*tx) / std::cos(*ty);
    *py += vvar * std::tan(*ty);
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarTangent>();
}

} // namespace apo
