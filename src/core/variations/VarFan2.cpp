#include "VarFan2.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

namespace {
// Delphi's Frac(X) = X - Trunc(X) (truncation toward zero, keeps sign).
double frac(double x) { return x - std::trunc(x); }
} // namespace

VarFan2::VarFan2() : fanX_(2 * constructionRandom01() - 1), fanY_(2 * constructionRandom01() - 1) {}

void VarFan2::prepare() {
    constexpr double kEps = 1e-10;
    constexpr double kPi = 3.14159265358979323846;
    dy_ = fanY_;
    dx_ = kPi * (fanX_ * fanX_ + kEps);
    dx2_ = dx_ / 2;
}

void VarFan2::calc() {
    // Delphi: arctan2(FTx^, FTy^) - Y=FTx, X=FTy (the "swapped" convention,
    // like XForm's own local Polar/Disc use - not the Y=FTy,X=FTx order
    // most other variations in this batch use). Direct translation:
    // atan2(*tx, *ty).
    const double angle = std::atan2(*tx, *ty);

    double fanAngle;
    if (frac((angle + dy_) / dx_) > 0.5) {
        fanAngle = angle - dx2_;
    } else {
        fanAngle = angle + dx2_;
    }

    const double sinr = std::sin(fanAngle), cosr = std::cos(fanAngle);
    const double r = vvar * std::sqrt((*tx) * (*tx) + (*ty) * (*ty));

    *px += r * cosr;
    *py += r * sinr;
    *pz += vvar * *tz;
}

std::string VarFan2::variableNameAt(int index) const {
    switch (index) {
        case 0: return "fan2_x";
        case 1: return "fan2_y";
        default: return "";
    }
}

bool VarFan2::getVariable(const std::string& name, double& value) const {
    if (name == "fan2_x") { value = fanX_; return true; }
    if (name == "fan2_y") { value = fanY_; return true; }
    return false;
}

bool VarFan2::setVariable(const std::string& name, double& value) {
    if (name == "fan2_x") { fanX_ = value; return true; }
    if (name == "fan2_y") { fanY_ = value; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarFan2>();
}

} // namespace apo
