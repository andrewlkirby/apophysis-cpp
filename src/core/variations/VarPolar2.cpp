#include "VarPolar2.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void VarPolar2::prepare() {
    p2vv_ = vvar / kPi;
    p2vv2_ = p2vv_ * 0.5;
}

void VarPolar2::calc() {
    *py += p2vv2_ * std::log((*tx) * (*tx) + (*ty) * (*ty));
    // Delphi: `ArcTan2(FTx^, FTy^)` - Y=FTx, X=FTy, SWAPPED from the usual
    // atan2(y,x) order (matches XForm's own local Polar/Disc precalcAngle,
    // unlike julian/ngon which use the un-swapped order - verified against
    // each source individually, not assumed consistent across files).
    *px += p2vv_ * std::atan2(*tx, *ty);
    *pz += vvar * (*tz);
}

// numVariables()/variableNameAt()/getVariable()/setVariable() intentionally
// not overridden: the base class defaults (0 variables, always-false
// accessors) already match TVariationPolar2's behavior exactly (it
// overrides them only to return the same "no parameters" answers).

namespace {
const bool kRegistered = registerVariation<VarPolar2>();
}

} // namespace apo
