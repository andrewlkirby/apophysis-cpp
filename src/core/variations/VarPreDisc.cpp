#include "VarPreDisc.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void VarPreDisc::prepare() { vvarByPi_ = vvar / kPi; }

void VarPreDisc::calc() {
    const double t = kPi * std::sqrt((*tx) * (*tx) + (*ty) * (*ty));
    const double sinr = std::sin(t), cosr = std::cos(t);
    // arctan2(FTx^, FTy^) - Y=*tx, X=*ty, SAME swapped convention as
    // XForm's own local Disc variation (not julian's normal atan2(y,x)).
    const double r = vvarByPi_ * std::atan2(*tx, *ty);
    *tx = sinr * r;
    *ty = cosr * r;
    *tz = vvar * *tz;
}

namespace {
const bool kRegistered = registerVariation<VarPreDisc>();
}

} // namespace apo
