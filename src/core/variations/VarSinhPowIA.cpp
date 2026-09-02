#include "VarSinhPowIA.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
// z^p via the principal branch, same r2/atan2 idiom as VarSinhPow.cpp/
// VarJulian.cpp.
void complexPow(double re, double im, double p, double& outRe, double& outIm) {
    const double r2 = re * re + im * im;
    const double mag = std::pow(r2, p / 2.0);
    const double ang = p * std::atan2(im, re);
    outRe = mag * std::cos(ang);
    outIm = mag * std::sin(ang);
}
} // namespace

void VarSinhPowIA::calc() {
    // K = sinh(c^q) - see VarSinhPow.cpp's identical computation.
    double wRe, wIm;
    complexPow(cx_, cy_, q_, wRe, wIm);
    const double kRe = std::sinh(wRe) * std::cos(wIm);
    const double kIm = std::cosh(wRe) * std::sin(wIm);

    // T_c(re,im) = (re,im)^power + K.
    auto Tc = [&](double re, double im, double& outRe, double& outIm) {
        complexPow(re, im, power_, outRe, outIm);
        outRe += kRe;
        outIm += kIm;
    };

    const double zRe = *tx, zIm = *ty;
    double tcZRe, tcZIm;
    Tc(zRe, zIm, tcZRe, tcZIm);

    const double xRe = (alpha_ + beta_) * zRe + gamma_ * tcZRe;
    const double xIm = (alpha_ + beta_) * zIm + gamma_ * tcZIm;
    double tcXRe, tcXIm;
    Tc(xRe, xIm, tcXRe, tcXIm);

    const double sRe = (1.0 - lambda_) * tcXRe + lambda_ * tcZRe;
    const double sIm = (1.0 - lambda_) * tcXIm + lambda_ * tcZIm;
    double yRe, yIm;
    Tc(sRe, sIm, yRe, yIm);

    double zNextRe, zNextIm;
    Tc(yRe, yIm, zNextRe, zNextIm);

    *px += vvar * zNextRe;
    *py += vvar * zNextIm;
    *pz += vvar * (*tz);
}

std::string VarSinhPowIA::variableNameAt(int index) const {
    switch (index) {
        case 0: return "sinhpowia_power";
        case 1: return "sinhpowia_q";
        case 2: return "sinhpowia_cx";
        case 3: return "sinhpowia_cy";
        case 4: return "sinhpowia_alpha";
        case 5: return "sinhpowia_beta";
        case 6: return "sinhpowia_gamma";
        case 7: return "sinhpowia_lambda";
        default: return "";
    }
}

bool VarSinhPowIA::getVariable(const std::string& name, double& value) const {
    if (name == "sinhpowia_power") { value = power_; return true; }
    if (name == "sinhpowia_q") { value = q_; return true; }
    if (name == "sinhpowia_cx") { value = cx_; return true; }
    if (name == "sinhpowia_cy") { value = cy_; return true; }
    if (name == "sinhpowia_alpha") { value = alpha_; return true; }
    if (name == "sinhpowia_beta") { value = beta_; return true; }
    if (name == "sinhpowia_gamma") { value = gamma_; return true; }
    if (name == "sinhpowia_lambda") { value = lambda_; return true; }
    return false;
}

bool VarSinhPowIA::setVariable(const std::string& name, double& value) {
    if (name == "sinhpowia_power") { power_ = value; return true; }
    if (name == "sinhpowia_q") { q_ = value; return true; }
    if (name == "sinhpowia_cx") { cx_ = value; return true; }
    if (name == "sinhpowia_cy") { cy_ = value; return true; }
    if (name == "sinhpowia_alpha") { alpha_ = value; return true; }
    if (name == "sinhpowia_beta") { beta_ = value; return true; }
    if (name == "sinhpowia_gamma") { gamma_ = value; return true; }
    if (name == "sinhpowia_lambda") { lambda_ = value; return true; }
    return false;
}

bool VarSinhPowIA::resetVariable(const std::string& name) {
    if (name == "sinhpowia_power") { power_ = 2.0; return true; }
    if (name == "sinhpowia_q") { q_ = 1.0; return true; }
    if (name == "sinhpowia_cx") { cx_ = 0.3; return true; }
    if (name == "sinhpowia_cy") { cy_ = 0.2; return true; }
    if (name == "sinhpowia_alpha") { alpha_ = 0.3; return true; }
    if (name == "sinhpowia_beta") { beta_ = 0.3; return true; }
    if (name == "sinhpowia_gamma") { gamma_ = 0.4; return true; }
    if (name == "sinhpowia_lambda") { lambda_ = 0.25; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarSinhPowIA>();
}

} // namespace apo
