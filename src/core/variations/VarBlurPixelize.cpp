#include "VarBlurPixelize.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarBlurPixelize::prepare() {
    invSize_ = 1.0 / size_;
    v_ = vvar * size_;
}

void VarBlurPixelize::calc() {
    const double x = std::floor(*tx * invSize_);
    const double y = std::floor(*ty * invSize_);

    *px += v_ * (x + scale_ * (rng->uniform01() - 0.5) + 0.5);
    *py += v_ * (y + scale_ * (rng->uniform01() - 0.5) + 0.5);
    *pz += vvar * *tz;
}

std::string VarBlurPixelize::variableNameAt(int index) const {
    switch (index) {
        case 0: return "blur_pixelize_size";
        case 1: return "blur_pixelize_scale";
        default: return "";
    }
}

bool VarBlurPixelize::getVariable(const std::string& name, double& value) const {
    if (name == "blur_pixelize_size") { value = size_; return true; }
    if (name == "blur_pixelize_scale") { value = scale_; return true; }
    return false;
}

bool VarBlurPixelize::setVariable(const std::string& name, double& value) {
    if (name == "blur_pixelize_size") {
        if (value < 1e-6) value = 1e-6;
        size_ = value;
        return true;
    }
    if (name == "blur_pixelize_scale") {
        scale_ = value;
        return true;
    }
    return false;
}

bool VarBlurPixelize::resetVariable(const std::string& name) {
    if (name == "blur_pixelize_size") { size_ = 0.1; return true; }
    if (name == "blur_pixelize_scale") { scale_ = 1; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarBlurPixelize>();
}

} // namespace apo
