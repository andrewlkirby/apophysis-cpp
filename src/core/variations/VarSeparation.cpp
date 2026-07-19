#include "VarSeparation.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarSeparation::calc() {
    if (*tx > 0.0) {
        *px += vvar * (std::sqrt((*tx) * (*tx) + separationX_ * separationX_) - *tx * separationXInside_);
    } else {
        *px -= vvar * (std::sqrt((*tx) * (*tx) + separationX_ * separationX_) + *tx * separationXInside_);
    }
    if (*ty > 0.0) {
        *py += vvar * (std::sqrt((*ty) * (*ty) + separationY_ * separationY_) - *ty * separationYInside_);
    } else {
        *py -= vvar * (std::sqrt((*ty) * (*ty) + separationY_ * separationY_) + *ty * separationYInside_);
    }

    *pz += vvar * *tz;
}

std::string VarSeparation::variableNameAt(int index) const {
    switch (index) {
        case 0: return "separation_x";
        case 1: return "separation_y";
        case 2: return "separation_xinside";
        case 3: return "separation_yinside";
        default: return "";
    }
}

bool VarSeparation::getVariable(const std::string& name, double& value) const {
    if (name == "separation_x") { value = separationX_; return true; }
    if (name == "separation_y") { value = separationY_; return true; }
    if (name == "separation_xinside") { value = separationXInside_; return true; }
    if (name == "separation_yinside") { value = separationYInside_; return true; }
    return false;
}

bool VarSeparation::setVariable(const std::string& name, double& value) {
    if (name == "separation_x") { separationX_ = value; return true; }
    if (name == "separation_y") { separationY_ = value; return true; }
    if (name == "separation_xinside") { separationXInside_ = value; return true; }
    if (name == "separation_yinside") { separationYInside_ = value; return true; }
    return false;
}

bool VarSeparation::resetVariable(const std::string& name) {
    if (name == "separation_x") { separationX_ = 1; return true; }
    if (name == "separation_y") { separationY_ = 1; return true; }
    if (name == "separation_xinside") { separationXInside_ = 0; return true; }
    if (name == "separation_yinside") { separationYInside_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarSeparation>();
}

} // namespace apo
