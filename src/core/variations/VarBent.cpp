#include "VarBent.h"

#include "../VariationRegistration.h"

namespace apo {

void VarBent::calc() {
    double nx = *tx;
    double ny = *ty;

    if (nx < 0.0) nx *= 2.0;
    if (ny < 0.0) ny /= 2.0;

    *px += vvar * nx;
    *py += vvar * ny;
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarBent>();
}

} // namespace apo
