#include "VarJulian.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

// Matches `N := random(5) + 2;` in the original's Create.
VarJulian::VarJulian() : n_(static_cast<int>(constructionRandom01() * 5) + 2) {}

void VarJulian::prepare() {
    absN_ = std::abs(n_);
    cN_ = c_ / n_ / 2.0;
    vvar2_ = vvar * std::sqrt(2.0) / 2.0;
}

void VarJulian::calc() {
    // Delphi: `arctan2(FTy^, FTx^)` - ArcTan2(Y, X) has the same argument
    // order as C's atan2(y, x), so this is a direct translation, not a
    // swap (unlike XForm's own Polar/Disc, which call arctan2(FTx, FTy)).
    const double angle = (std::atan2(*ty, *tx) + kTwoPi * rng->uniformInt(absN_)) / n_;
    const double sina = std::sin(angle);
    const double cosa = std::cos(angle);
    const double r = vvar * std::pow((*tx) * (*tx) + (*ty) * (*ty), cN_);

    *px += r * cosa;
    *py += r * sina;
    *pz += vvar * *tz;
}

void VarJulian::calcPower1() {
    *px += vvar * *tx;
    *py += vvar * *ty;
    *pz += vvar * *tz;
}

void VarJulian::calcPowerMinus1() {
    const double r = vvar / ((*tx) * (*tx) + (*ty) * (*ty));
    *px += r * *tx;
    *py -= r * *ty;
    *pz += vvar * *tz;
}

void VarJulian::calcPower2() {
    const double dist = std::sqrt(std::sqrt((*tx) * (*tx) + (*ty) * (*ty)) + *tx);
    if (rng->uniformInt(2) == 0) {
        *px += vvar2_ * dist;
        *py += vvar2_ / dist * (*ty);
    } else {
        *px -= vvar2_ * dist;
        *py -= vvar2_ / dist * (*ty);
    }
    *pz += vvar * *tz;
}

void VarJulian::calcPowerMinus2() {
    double r = std::sqrt((*tx) * (*tx) + (*ty) * (*ty));
    const double xd = r + *tx;
    r = vvar / std::sqrt(r * ((*ty) * (*ty) + xd * xd));
    if (rng->uniformInt(2) == 0) {
        *px += r * xd;
        *py -= r * (*ty);
    } else {
        *px -= r * xd;
        *py += r * (*ty);
    }
    *pz += vvar * *tz;
}

CalcFn VarJulian::selectCalcFunction() {
    if (c_ == 1) {
        if (n_ == 2) return bindCalc<VarJulian, &VarJulian::calcPower2>(this);
        if (n_ == -2) return bindCalc<VarJulian, &VarJulian::calcPowerMinus2>(this);
        if (n_ == 1) return bindCalc<VarJulian, &VarJulian::calcPower1>(this);
        if (n_ == -1) return bindCalc<VarJulian, &VarJulian::calcPowerMinus1>(this);
    }
    return bindCalc<VarJulian, &VarJulian::calc>(this);
}

std::string VarJulian::variableNameAt(int index) const {
    switch (index) {
        case 0: return "julian_power";
        case 1: return "julian_dist";
        default: return "";
    }
}

bool VarJulian::getVariable(const std::string& name, double& value) const {
    if (name == "julian_power") { value = n_; return true; }
    if (name == "julian_dist") { value = c_; return true; }
    return false;
}

bool VarJulian::setVariable(const std::string& name, double& value) {
    if (name == "julian_power") {
        n_ = static_cast<int>(std::lround(value));
        if (n_ == 0) n_ = 1;
        value = n_;
        return true;
    }
    if (name == "julian_dist") {
        c_ = value;
        return true;
    }
    return false;
}

bool VarJulian::resetVariable(const std::string& name) {
    if (name == "julian_power") {
        n_ = (n_ == 2) ? -2 : 2;
        return true;
    }
    if (name == "julian_dist") {
        c_ = 1;
        return true;
    }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarJulian>();
}

} // namespace apo
