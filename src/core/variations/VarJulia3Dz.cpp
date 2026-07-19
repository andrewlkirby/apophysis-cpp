#include "VarJulia3Dz.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

// Matches `N := random(5) + 2; if random(2) = 0 then N := -N;`.
VarJulia3Dz::VarJulia3Dz() : n_(static_cast<int>(constructionRandom01() * 5) + 2) {
    if (constructionRandom01() < 0.5) n_ = -n_;
}

void VarJulia3Dz::prepare() {
    absN_ = std::abs(n_);
    cN_ = 1.0 / n_ / 2.0;
}

void VarJulia3Dz::calc() {
    const double r2d = (*tx) * (*tx) + (*ty) * (*ty);
    const double r = vvar * std::pow(r2d, cN_);

    *pz += r * *tz / (std::sqrt(r2d) * absN_);

    const double angle = (std::atan2(*ty, *tx) + kTwoPi * rng->uniformInt(absN_)) / n_;
    *px += r * std::cos(angle);
    *py += r * std::sin(angle);
}

void VarJulia3Dz::calcPower2() {
    // r2d here is already the square root (sqrt(x^2+y^2)) - a different
    // quantity than calc()'s r2d (which is the unrooted sum) - naming kept
    // as in the Pascal source rather than disambiguated, since that's what
    // the original literally computes in this branch.
    const double r2d = std::sqrt((*tx) * (*tx) + (*ty) * (*ty));
    const double r = vvar * std::sqrt(r2d);

    *pz += r * *tz / r2d / 2;

    const double angle = std::atan2(*ty, *tx) / 2 + kPi * rng->uniformInt(2);
    *px += r * std::cos(angle);
    *py += r * std::sin(angle);
}

void VarJulia3Dz::calcPowerMinus2() {
    const double r2d = std::sqrt((*tx) * (*tx) + (*ty) * (*ty));
    const double r = vvar / std::sqrt(r2d);

    *pz += r * *tz / r2d / 2;

    // Angle formula and Py sign both differ from VarJulia3Djf's
    // CalcPowerMinus2 (which is `angle/2 + pi*random(2)` with a `-=` on Py)
    // - this one is `pi*random(2) - angle/2` with a `+=` on Py. Ported
    // exactly as written, not assumed symmetric with the sibling variation.
    const double angle = kPi * rng->uniformInt(2) - std::atan2(*ty, *tx) / 2;
    *px += r * std::cos(angle);
    *py += r * std::sin(angle);
}

void VarJulia3Dz::calcPower1() {
    *px += vvar * *tx;
    *py += vvar * *ty;
    *pz += vvar * *tz;
}

void VarJulia3Dz::calcPowerMinus1() {
    // No z-term in the denominator here, unlike VarJulia3Djf's
    // CalcPowerMinus1 (which divides by x^2+y^2+z^2) - this one is purely
    // 2D in its denominator despite still updating Pz.
    const double r = vvar / ((*tx) * (*tx) + (*ty) * (*ty));
    *px += r * *tx;
    *py -= r * *ty;
    *pz += r * *tz;
}

CalcFn VarJulia3Dz::selectCalcFunction() {
    if (n_ == 2) return bindCalc<VarJulia3Dz, &VarJulia3Dz::calcPower2>(this);
    if (n_ == -2) return bindCalc<VarJulia3Dz, &VarJulia3Dz::calcPowerMinus2>(this);
    if (n_ == 1) return bindCalc<VarJulia3Dz, &VarJulia3Dz::calcPower1>(this);
    if (n_ == -1) return bindCalc<VarJulia3Dz, &VarJulia3Dz::calcPowerMinus1>(this);
    return bindCalc<VarJulia3Dz, &VarJulia3Dz::calc>(this);
}

std::string VarJulia3Dz::variableNameAt(int index) const {
    return index == 0 ? "julia3Dz_power" : "";
}

bool VarJulia3Dz::getVariable(const std::string& name, double& value) const {
    if (name == "julia3Dz_power") { value = n_; return true; }
    return false;
}

bool VarJulia3Dz::setVariable(const std::string& name, double& value) {
    if (name == "julia3Dz_power") {
        n_ = static_cast<int>(std::lround(value));
        if (n_ == 0) n_ = 1;
        value = n_;
        return true;
    }
    return false;
}

bool VarJulia3Dz::resetVariable(const std::string& name) {
    if (name == "julia3Dz_power") {
        n_ = (n_ == 2) ? -2 : 2;
        return true;
    }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarJulia3Dz>();
}

} // namespace apo
