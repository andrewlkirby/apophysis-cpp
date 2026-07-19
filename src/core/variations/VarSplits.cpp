#include "VarSplits.h"

#include "../VariationRegistration.h"

namespace apo {

void VarSplits::calc() {
    if (*tx >= 0.0) {
        *px += vvar * (*tx + splitsX_);
    } else {
        *px += vvar * (*tx - splitsX_);
    }
    if (*ty >= 0.0) {
        *py += vvar * (*ty + splitsY_);
    } else {
        *py += vvar * (*ty - splitsY_);
    }
    *pz += vvar * *tz;
}

std::string VarSplits::variableNameAt(int index) const {
    switch (index) {
        case 0: return "splits_x";
        case 1: return "splits_y";
        default: return "";
    }
}

bool VarSplits::getVariable(const std::string& name, double& value) const {
    if (name == "splits_x") { value = splitsX_; return true; }
    if (name == "splits_y") { value = splitsY_; return true; }
    return false;
}

bool VarSplits::setVariable(const std::string& name, double& value) {
    if (name == "splits_x") { splitsX_ = value; return true; }
    if (name == "splits_y") { splitsY_ = value; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarSplits>();
}

} // namespace apo
