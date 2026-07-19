#include "VarElliptic.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
double sqrtSafe(double x) { return x < 0.0 ? 0.0 : std::sqrt(x); }
} // namespace

void VarElliptic::prepare() {
    constexpr double kPi = 3.14159265358979323846;
    v_ = vvar / (kPi / 2.0);
}

void VarElliptic::calc() {
    const double tmp = (*ty) * (*ty) + (*tx) * (*tx) + 1.0;
    const double x2 = 2.0 * (*tx);
    const double xmax = 0.5 * (std::sqrt(tmp + x2) + std::sqrt(tmp - x2));

    const double ea = (*tx) / xmax;
    const double eb = sqrtSafe(1.0 - ea * ea);

    *pz += vvar * *tz;
    // Delphi: ArcTan2(a, b) - ArcTan2(Y, X) matches C's atan2(y, x) in
    // argument order, so this is ea, eb directly, not swapped.
    *px += v_ * std::atan2(ea, eb);

    if (*ty > 0) {
        *py += v_ * std::log(xmax + sqrtSafe(xmax - 1.0));
    } else {
        *py -= v_ * std::log(xmax + sqrtSafe(xmax - 1.0));
    }
}

namespace {
const bool kRegistered = registerVariation<VarElliptic>();
}

} // namespace apo
