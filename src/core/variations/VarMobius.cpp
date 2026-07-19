#include "VarMobius.h"

#include "../VariationRegistration.h"

namespace apo {

void VarMobius::calc() {
    const double uRe = reA_ * (*tx) - imA_ * (*ty) + reB_;
    const double uIm = reA_ * (*ty) + imA_ * (*tx) + imB_;
    const double vRe = reC_ * (*tx) - imC_ * (*ty) + reD_;
    const double vIm = reC_ * (*ty) + imC_ * (*tx) + imD_;

    const double vDenom = vRe * vRe + vIm * vIm;

    *px += vvar * (uRe * vRe + uIm * vIm) / vDenom;
    *py += vvar * (uIm * vRe - uRe * vIm) / vDenom;
    *pz += vvar * (*tz);
}

std::string VarMobius::variableNameAt(int index) const {
    switch (index) {
        case 0: return "Re_A";
        case 1: return "Im_A";
        case 2: return "Re_B";
        case 3: return "Im_B";
        case 4: return "Re_C";
        case 5: return "Im_C";
        case 6: return "Re_D";
        case 7: return "Im_D";
        default: return "";
    }
}

bool VarMobius::getVariable(const std::string& name, double& value) const {
    if (name == "Re_A") { value = reA_; return true; }
    if (name == "Im_A") { value = imA_; return true; }
    if (name == "Re_B") { value = reB_; return true; }
    if (name == "Im_B") { value = imB_; return true; }
    if (name == "Re_C") { value = reC_; return true; }
    if (name == "Im_C") { value = imC_; return true; }
    if (name == "Re_D") { value = reD_; return true; }
    if (name == "Im_D") { value = imD_; return true; }
    return false;
}

bool VarMobius::setVariable(const std::string& name, double& value) {
    if (name == "Re_A") { reA_ = value; return true; }
    if (name == "Im_A") { imA_ = value; return true; }
    if (name == "Re_B") { reB_ = value; return true; }
    if (name == "Im_B") { imB_ = value; return true; }
    if (name == "Re_C") { reC_ = value; return true; }
    if (name == "Im_C") { imC_ = value; return true; }
    if (name == "Re_D") { reD_ = value; return true; }
    if (name == "Im_D") { imD_ = value; return true; }
    return false;
}

bool VarMobius::resetVariable(const std::string& name) {
    if (name == "Re_A") { reA_ = 1; return true; }
    if (name == "Im_A") { imA_ = 0; return true; }
    if (name == "Re_B") { reB_ = 0; return true; }
    if (name == "Im_B") { imB_ = 0; return true; }
    if (name == "Re_C") { reC_ = 0; return true; }
    if (name == "Im_C") { imC_ = 0; return true; }
    if (name == "Re_D") { reD_ = 1; return true; }
    if (name == "Im_D") { imD_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarMobius>();
}

} // namespace apo
