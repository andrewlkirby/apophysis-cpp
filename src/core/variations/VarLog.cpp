#include "VarLog.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarLog::prepare() { denom_ = 0.5 / std::log(base_); }

void VarLog::calc() {
    *px += vvar * std::log((*tx) * (*tx) + (*ty) * (*ty)) * denom_;
    *py += vvar * std::atan2(*ty, *tx);
    *pz += vvar * *tz;
}

std::string VarLog::variableNameAt(int index) const { return index == 0 ? "log_base" : ""; }

bool VarLog::getVariable(const std::string& name, double& value) const {
    if (name == "log_base") { value = base_; return true; }
    return false;
}

bool VarLog::setVariable(const std::string& name, double& value) {
    if (name == "log_base") {
        base_ = value;
        if (base_ < 1e-6) base_ = 1e-6;
        value = base_;
        return true;
    }
    return false;
}

bool VarLog::resetVariable(const std::string& name) {
    if (name == "log_base") {
        base_ = 2.71828182845905;
        return true;
    }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarLog>();
}

} // namespace apo
