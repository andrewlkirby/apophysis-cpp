#include "VarJuliaScope.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

// Matches `N := random(5) + 2; c := 1.0;`.
VarJuliaScope::VarJuliaScope() : n_(static_cast<int>(constructionRandom01() * 5) + 2) {}

void VarJuliaScope::prepare() {
    rN_ = std::abs(n_);
    cn_ = dist_ / n_ / 2.0;
}

void VarJuliaScope::calc() {
    const int rnd = rng->uniformInt(rN_);
    const double baseAngle = std::atan2(*ty, *tx);
    // The mirroring branch is chosen by the parity of `rnd` itself (not a
    // separate coin flip), matching `if (rnd and 1) = 0 then ... else ...`.
    const double angle = ((rnd & 1) == 0) ? (kTwoPi * rnd + baseAngle) / n_ : (kTwoPi * rnd - baseAngle) / n_;
    const double r = vvar * std::pow((*tx) * (*tx) + (*ty) * (*ty), cn_);

    *px += r * std::cos(angle);
    *py += r * std::sin(angle);
    *pz += vvar * *tz;
}

void VarJuliaScope::calcPower2() {
    const double baseAngle = std::atan2(*ty, *tx) / 2;
    const double angle = (rng->uniformInt(2) == 0) ? baseAngle : (kPi - baseAngle);
    const double r = vvar * std::sqrt(std::sqrt((*tx) * (*tx) + (*ty) * (*ty)));

    *px += r * std::cos(angle);
    *py += r * std::sin(angle);
    *pz += vvar * *tz;
}

void VarJuliaScope::calcPowerMinus2() {
    const double baseAngle = std::atan2(*ty, *tx) / 2;
    const double angle = (rng->uniformInt(2) == 0) ? baseAngle : (kPi - baseAngle);
    const double r = vvar / std::sqrt(std::sqrt((*tx) * (*tx) + (*ty) * (*ty)));

    *px += r * std::cos(angle);
    *py -= r * std::sin(angle);
    *pz += vvar * *tz;
}

void VarJuliaScope::calcPower1() {
    *px += vvar * *tx;
    *py += vvar * *ty;
    *pz += vvar * *tz;
}

void VarJuliaScope::calcPowerMinus1() {
    const double r = vvar / ((*tx) * (*tx) + (*ty) * (*ty));
    *px += r * *tx;
    *py -= r * *ty;
    *pz += vvar * *tz;
}

CalcFn VarJuliaScope::selectCalcFunction() {
    if (dist_ == 1) {
        if (n_ == 2) return bindCalc<VarJuliaScope, &VarJuliaScope::calcPower2>(this);
        if (n_ == -2) return bindCalc<VarJuliaScope, &VarJuliaScope::calcPowerMinus2>(this);
        if (n_ == 1) return bindCalc<VarJuliaScope, &VarJuliaScope::calcPower1>(this);
        if (n_ == -1) return bindCalc<VarJuliaScope, &VarJuliaScope::calcPowerMinus1>(this);
    }
    return bindCalc<VarJuliaScope, &VarJuliaScope::calc>(this);
}

std::string VarJuliaScope::variableNameAt(int index) const {
    switch (index) {
        case 0: return "juliascope_power";
        case 1: return "juliascope_dist";
        default: return "";
    }
}

bool VarJuliaScope::getVariable(const std::string& name, double& value) const {
    if (name == "juliascope_power") { value = n_; return true; }
    if (name == "juliascope_dist") { value = dist_; return true; }
    return false;
}

bool VarJuliaScope::setVariable(const std::string& name, double& value) {
    if (name == "juliascope_power") {
        n_ = static_cast<int>(std::lround(value));
        if (n_ == 0) n_ = 1;
        value = n_;
        return true;
    }
    if (name == "juliascope_dist") {
        dist_ = value;
        return true;
    }
    return false;
}

bool VarJuliaScope::resetVariable(const std::string& name) {
    if (name == "juliascope_power") {
        n_ = (n_ == 2) ? -2 : 2;
        return true;
    }
    if (name == "juliascope_dist") {
        dist_ = 1;
        return true;
    }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarJuliaScope>();
}

} // namespace apo
