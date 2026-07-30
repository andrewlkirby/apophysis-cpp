#include "VarTwintrian.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;

// flam3's badvalue(x) macro: catches NaN and magnitudes that have blown up
// past a sane range (including +/-inf, since those compare true against
// +/-1e10 too) - log10(0) below hits exactly this when sinr is 0.
bool isBadValue(double x) { return std::isnan(x) || x > 1e10 || x < -1e10; }
} // namespace

void VarTwintrian::calc() {
    const double rad = std::sqrt(*tx * *tx + *ty * *ty);
    const double r = rng->uniform01() * vvar * rad;
    const double sinr = std::sin(r), cosr = std::cos(r);

    double diff = std::log10(sinr * sinr) + cosr;
    if (isBadValue(diff)) diff = -30.0;

    *px += vvar * *tx * diff;
    *py += vvar * *tx * (diff - sinr * kPi);
    *pz += vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarTwintrian>();
}

} // namespace apo
