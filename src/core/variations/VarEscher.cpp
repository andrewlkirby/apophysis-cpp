#include "VarEscher.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
// Delphi's Frac(X) = X - Trunc(X) (truncation toward zero, keeps sign).
double frac(double x) { return x - std::trunc(x); }
} // namespace

void VarEscher::prepare() {
    // Delphi: `sincos(escher_beta, d, c);` - SinCos(angle, out Sin, out Cos)
    // writes sin into the FIRST out-param and cos into the second, so this
    // assigns escD_ = sin(escherBeta_), escC_ = cos(escherBeta_) - not the
    // other way around.
    escD_ = std::sin(escherBeta_);
    escC_ = std::cos(escherBeta_);
    escC_ = 0.5 * (1.0 + escC_);
    escD_ = 0.5 * escD_;
}

void VarEscher::calc() {
    // Delphi: arctan2(FTy^, FTx^) - Y=FTy, X=FTx, same order as atan2(y, x).
    const double ang = std::atan2(*ty, *tx);
    const double lnr = 0.5 * std::log((*tx) * (*tx) + (*ty) * (*ty));

    const double m = vvar * std::exp(escC_ * lnr - escD_ * ang);

    const double angle = escC_ * ang + escD_ * lnr;
    const double sn = std::sin(angle);
    const double cs = std::cos(angle);

    *px += m * cs;
    *py += m * sn;
    *pz += vvar * *tz;
}

std::string VarEscher::variableNameAt(int index) const {
    switch (index) {
        case 0: return "escher_beta";
        default: return "";
    }
}

bool VarEscher::getVariable(const std::string& name, double& value) const {
    if (name == "escher_beta") { value = escherBeta_; return true; }
    return false;
}

bool VarEscher::setVariable(const std::string& name, double& value) {
    if (name == "escher_beta") {
        constexpr double kPi = 3.14159265358979323846;
        value = frac((value + kPi) / (2 * kPi)) * 2 * kPi - kPi;
        escherBeta_ = value;
        return true;
    }
    return false;
}

bool VarEscher::resetVariable(const std::string& name) {
    if (name == "escher_beta") { escherBeta_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarEscher>();
}

} // namespace apo
