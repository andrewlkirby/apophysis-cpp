#include "VarBlurZoom.h"

#include "../VariationRegistration.h"

namespace apo {

void VarBlurZoom::calc() {
    const double z = 1.0 + length_ * rng->uniform01();
    *px += vvar * ((*tx - x_) * z + x_);
    *py += vvar * ((*ty - y_) * z - y_);
    *pz += vvar * *tz;
}

std::string VarBlurZoom::variableNameAt(int index) const {
    switch (index) {
        case 0: return "blur_zoom_length";
        case 1: return "blur_zoom_x";
        case 2: return "blur_zoom_y";
        default: return "";
    }
}

bool VarBlurZoom::getVariable(const std::string& name, double& value) const {
    if (name == "blur_zoom_length") { value = length_; return true; }
    if (name == "blur_zoom_x") { value = x_; return true; }
    if (name == "blur_zoom_y") { value = y_; return true; }
    return false;
}

bool VarBlurZoom::setVariable(const std::string& name, double& value) {
    if (name == "blur_zoom_length") { length_ = value; return true; }
    if (name == "blur_zoom_x") { x_ = value; return true; }
    if (name == "blur_zoom_y") { y_ = value; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarBlurZoom>();
}

} // namespace apo
