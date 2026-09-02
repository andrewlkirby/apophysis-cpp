#include "VarSinhPow.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarSinhPow::calc() {
    // K = sinh(c^q), a constant complex number depending only on this
    // xform's own parameters (not the point) - see VarSinhPow.h's doc
    // comment on why this is recomputed here instead of cached in
    // prepare(). c^q via the principal branch of complex exponentiation,
    // same r2/atan2 idiom VarJulian::calc() uses for its own r^cN term.
    const double cr2 = cx_ * cx_ + cy_ * cy_;
    const double cMag = std::pow(cr2, q_ / 2.0);
    const double cAngle = q_ * std::atan2(cy_, cx_);
    const double wRe = cMag * std::cos(cAngle);
    const double wIm = cMag * std::sin(cAngle);
    const double kRe = std::sinh(wRe) * std::cos(wIm);
    const double kIm = std::cosh(wRe) * std::sin(wIm);

    // z^p, z = (tx, ty), same principal-branch/r2 idiom.
    const double zr2 = (*tx) * (*tx) + (*ty) * (*ty);
    const double zMag = std::pow(zr2, power_ / 2.0);
    const double zAngle = power_ * std::atan2(*ty, *tx);

    *px += vvar * (zMag * std::cos(zAngle) + kRe);
    *py += vvar * (zMag * std::sin(zAngle) + kIm);
    *pz += vvar * (*tz);
}

std::string VarSinhPow::variableNameAt(int index) const {
    switch (index) {
        case 0: return "sinhpow_power";
        case 1: return "sinhpow_q";
        case 2: return "sinhpow_cx";
        case 3: return "sinhpow_cy";
        default: return "";
    }
}

bool VarSinhPow::getVariable(const std::string& name, double& value) const {
    if (name == "sinhpow_power") { value = power_; return true; }
    if (name == "sinhpow_q") { value = q_; return true; }
    if (name == "sinhpow_cx") { value = cx_; return true; }
    if (name == "sinhpow_cy") { value = cy_; return true; }
    return false;
}

bool VarSinhPow::setVariable(const std::string& name, double& value) {
    if (name == "sinhpow_power") { power_ = value; return true; }
    if (name == "sinhpow_q") { q_ = value; return true; }
    if (name == "sinhpow_cx") { cx_ = value; return true; }
    if (name == "sinhpow_cy") { cy_ = value; return true; }
    return false;
}

bool VarSinhPow::resetVariable(const std::string& name) {
    if (name == "sinhpow_power") { power_ = 2.0; return true; }
    if (name == "sinhpow_q") { q_ = 1.0; return true; }
    if (name == "sinhpow_cx") { cx_ = 1.0; return true; }
    if (name == "sinhpow_cy") { cy_ = 0.0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarSinhPow>();
}

} // namespace apo
