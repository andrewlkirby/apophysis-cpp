#include "VarCrop.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarCrop::prepare() {
    if (x0_ < x1_) {
        normX0_ = x0_;
        normX1_ = x1_;
    } else {
        normX0_ = x1_;
        normX1_ = x0_;
    }

    if (y0_ < y1_) {
        normY0_ = y0_;
        normY1_ = y1_;
    } else {
        normY0_ = y1_;
        normY1_ = y0_;
    }

    w_ = (normX1_ - normX0_) * 0.5 * s_;
    h_ = (normY1_ - normY0_) * 0.5 * s_;
}

void VarCrop::calc() {
    double x = *tx;
    double y = *ty;

    if ((x < normX0_ || x > normX1_ || y < normY0_ || y > normY1_) && z_ != 0) {
        x = 0;
        y = 0;
    } else {
        if (x < normX0_) x = normX0_ + rng->uniform01() * w_;
        else if (x > normX1_) x = normX1_ - rng->uniform01() * w_;
        if (y < normY0_) y = normY0_ + rng->uniform01() * h_;
        else if (y > normY1_) y = normY1_ - rng->uniform01() * h_;
    }

    *px += vvar * x;
    *py += vvar * y;
    *pz += vvar * *tz;
}

std::string VarCrop::variableNameAt(int index) const {
    switch (index) {
        case 0: return "crop_left";
        case 1: return "crop_top";
        case 2: return "crop_right";
        case 3: return "crop_bottom";
        case 4: return "crop_scatter_area";
        case 5: return "crop_zero";
        default: return "";
    }
}

bool VarCrop::getVariable(const std::string& name, double& value) const {
    if (name == "crop_left") { value = x0_; return true; }
    if (name == "crop_top") { value = y0_; return true; }
    if (name == "crop_right") { value = x1_; return true; }
    if (name == "crop_bottom") { value = y1_; return true; }
    if (name == "crop_scatter_area") { value = s_; return true; }
    if (name == "crop_zero") { value = z_; return true; }
    return false;
}

bool VarCrop::setVariable(const std::string& name, double& value) {
    if (name == "crop_left") { x0_ = value; return true; }
    if (name == "crop_top") { y0_ = value; return true; }
    if (name == "crop_right") { x1_ = value; return true; }
    if (name == "crop_bottom") { y1_ = value; return true; }
    if (name == "crop_scatter_area") {
        if (value < -1) value = -1;
        if (value > 1) value = 1;
        s_ = value;
        return true;
    }
    if (name == "crop_zero") {
        if (value > 1) value = 1;
        if (value < 0) value = 0;
        // z_ is Round() of the now-clamped value; `value` itself is left as
        // the clamped float (matching the original, which writes back the
        // clamped-but-unrounded Value, not the rounded integer).
        z_ = static_cast<int>(std::lround(value));
        return true;
    }
    return false;
}

bool VarCrop::resetVariable(const std::string& name) {
    if (name == "crop_left") { x0_ = -1; return true; }
    if (name == "crop_top") { y0_ = -1; return true; }
    if (name == "crop_right") { x1_ = 1; return true; }
    if (name == "crop_bottom") { y1_ = 1; return true; }
    if (name == "crop_scatter_area") { s_ = 0; return true; }
    if (name == "crop_zero") { z_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarCrop>();
}

} // namespace apo
