#include "VarFoci.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarFoci::calc() {
    const double expx = std::exp(*tx) * 0.5;
    const double expnx = 0.25 / expx;
    const double siny = std::sin(*ty), cosy = std::cos(*ty);

    double tmp = expx + expnx - cosy;
    // Exact-equality check in the original (`if (tmp = 0)`), preserved as-is.
    if (tmp == 0) tmp = 1e-6;
    tmp = vvar / tmp;

    *px += (expx - expnx) * tmp;
    *py += siny * tmp;
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarFoci>();
}

} // namespace apo
