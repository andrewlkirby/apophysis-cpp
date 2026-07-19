#include "VarJulia3Djf.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

// Matches `N := random(5) + 2; if random(2) = 0 then N := -N;` - unlike
// VarJulian, this variation's N ranges over +-[2,6], not just [2,7].
VarJulia3Djf::VarJulia3Djf() : n_(static_cast<int>(constructionRandom01() * 5) + 2) {
    if (constructionRandom01() < 0.5) n_ = -n_;
}

void VarJulia3Djf::prepare() {
    absN_ = std::abs(n_);
    cN_ = (1.0 / n_ - 1.0) / 2.0;
}

void VarJulia3Djf::calc() {
    const double z = *tz / absN_;
    const double r2d = (*tx) * (*tx) + (*ty) * (*ty);
    const double r = vvar * std::pow(r2d + z * z, cN_);

    *pz += r * z;

    const double tmp = r * std::sqrt(r2d);
    const double angle = (std::atan2(*ty, *tx) + kTwoPi * rng->uniformInt(absN_)) / n_;
    *px += tmp * std::cos(angle);
    *py += tmp * std::sin(angle);
}

void VarJulia3Djf::calcPower2() {
    const double z = *tz / 2;
    const double r2d = (*tx) * (*tx) + (*ty) * (*ty);
    const double r = vvar / std::sqrt(std::sqrt(r2d + z * z));

    *pz += r * z;

    const double tmp = r * std::sqrt(r2d);
    const double angle = std::atan2(*ty, *tx) / 2 + kPi * rng->uniformInt(2);
    *px += tmp * std::cos(angle);
    *py += tmp * std::sin(angle);
}

void VarJulia3Djf::calcPowerMinus2() {
    const double z = *tz / 2;
    const double r2d = (*tx) * (*tx) + (*ty) * (*ty);
    const double r3d = std::sqrt(r2d + z * z);
    const double r = vvar / (std::sqrt(r3d) * r3d);

    *pz += r * z;

    const double tmp = r * std::sqrt(r2d);
    const double angle = std::atan2(*ty, *tx) / 2 + kPi * rng->uniformInt(2);
    *px += tmp * std::cos(angle);
    *py -= tmp * std::sin(angle);
}

void VarJulia3Djf::calcPower1() {
    *px += vvar * *tx;
    *py += vvar * *ty;
    *pz += vvar * *tz;
}

void VarJulia3Djf::calcPowerMinus1() {
    const double r = vvar / ((*tx) * (*tx) + (*ty) * (*ty) + (*tz) * (*tz));
    *px += r * *tx;
    *py -= r * *ty;
    *pz += r * *tz;
}

CalcFn VarJulia3Djf::selectCalcFunction() {
    // No `c == 1`-style gate here (unlike julian/juliascope) - this
    // variation has no second "dist" parameter, so N alone selects the
    // fast path, matching TVariationJulia3DJF.GetCalcFunction exactly.
    if (n_ == 2) return bindCalc<VarJulia3Djf, &VarJulia3Djf::calcPower2>(this);
    if (n_ == -2) return bindCalc<VarJulia3Djf, &VarJulia3Djf::calcPowerMinus2>(this);
    if (n_ == 1) return bindCalc<VarJulia3Djf, &VarJulia3Djf::calcPower1>(this);
    if (n_ == -1) return bindCalc<VarJulia3Djf, &VarJulia3Djf::calcPowerMinus1>(this);
    return bindCalc<VarJulia3Djf, &VarJulia3Djf::calc>(this);
}

std::string VarJulia3Djf::variableNameAt(int index) const {
    return index == 0 ? "julia3D_power" : "";
}

bool VarJulia3Djf::getVariable(const std::string& name, double& value) const {
    if (name == "julia3D_power") { value = n_; return true; }
    return false;
}

bool VarJulia3Djf::setVariable(const std::string& name, double& value) {
    if (name == "julia3D_power") {
        n_ = static_cast<int>(std::lround(value));
        if (n_ == 0) n_ = 1;
        value = n_;
        return true;
    }
    return false;
}

bool VarJulia3Djf::resetVariable(const std::string& name) {
    if (name == "julia3D_power") {
        n_ = (n_ == 2) ? -2 : 2;
        return true;
    }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarJulia3Djf>();
}

} // namespace apo
