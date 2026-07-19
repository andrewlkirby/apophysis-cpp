#include "VarWaves2.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarWaves2::calc() {
    *px += vvar * (*tx + scaleX_ * std::sin(*ty * freqX_));
    *py += vvar * (*ty + scaleY_ * std::sin(*tx * freqY_));
    *pz += vvar * (*tz + scaleZ_ * std::sin(std::sqrt((*tx) * (*tx) + (*ty) * (*ty)) * freqZ_));
}

std::string VarWaves2::variableNameAt(int index) const {
    switch (index) {
        case 0: return "waves2_freqx";
        case 1: return "waves2_freqy";
        case 2: return "waves2_freqz";
        case 3: return "waves2_scalex";
        case 4: return "waves2_scaley";
        case 5: return "waves2_scalez";
        default: return "";
    }
}

bool VarWaves2::getVariable(const std::string& name, double& value) const {
    if (name == "waves2_freqx") { value = freqX_; return true; }
    if (name == "waves2_freqy") { value = freqY_; return true; }
    if (name == "waves2_freqz") { value = freqZ_; return true; }
    if (name == "waves2_scalex") { value = scaleX_; return true; }
    if (name == "waves2_scaley") { value = scaleY_; return true; }
    if (name == "waves2_scalez") { value = scaleZ_; return true; }
    return false;
}

bool VarWaves2::setVariable(const std::string& name, double& value) {
    if (name == "waves2_freqx") { freqX_ = value; return true; }
    if (name == "waves2_freqy") { freqY_ = value; return true; }
    if (name == "waves2_freqz") { freqZ_ = value; return true; }
    if (name == "waves2_scalex") { scaleX_ = value; return true; }
    if (name == "waves2_scaley") { scaleY_ = value; return true; }
    if (name == "waves2_scalez") { scaleZ_ = value; return true; }
    return false;
}

bool VarWaves2::resetVariable(const std::string& name) {
    if (name == "waves2_freqx") { freqX_ = 2; return true; }
    if (name == "waves2_freqy") { freqY_ = 2; return true; }
    if (name == "waves2_freqz") { freqZ_ = 0; return true; }
    if (name == "waves2_scalex") { scaleX_ = 1; return true; }
    if (name == "waves2_scaley") { scaleY_ = 1; return true; }
    if (name == "waves2_scalez") { scaleZ_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarWaves2>();
}

} // namespace apo
