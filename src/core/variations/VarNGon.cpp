#include "VarNGon.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void VarNGon::prepare() {
    cpower_ = -0.5 * ngonPower_;
    csides_ = 2.0 * kPi / ngonSides_;
    csidesinv_ = 1.0 / csides_;
}

void VarNGon::calc() {
    double rFactor;
    if (*tx == 0 && *ty == 0) {
        rFactor = 0;
    } else {
        rFactor = std::pow((*tx) * (*tx) + (*ty) * (*ty), cpower_);
    }

    // Delphi: `ArcTan2(FTy^, FTx^)` - Y=FTy, X=FTx, the standard atan2(y,x)
    // argument order (not swapped).
    const double theta = std::atan2(*ty, *tx);

    double phi = theta - csides_ * std::floor(theta * csidesinv_);
    if (phi > 0.5 * csides_) phi -= csides_;

    const double amp = (ngonCorners_ * (1.0 / std::cos(phi) - 1.0) + ngonCircle_) * vvar * rFactor;

    *px += amp * (*tx);
    *py += amp * (*ty);
    *pz += vvar * (*tz);
}

std::string VarNGon::variableNameAt(int index) const {
    switch (index) {
        case 0: return "ngon_sides";
        case 1: return "ngon_power";
        case 2: return "ngon_circle";
        case 3: return "ngon_corners";
        default: return "";
    }
}

bool VarNGon::getVariable(const std::string& name, double& value) const {
    if (name == "ngon_sides") { value = ngonSides_; return true; }
    if (name == "ngon_power") { value = ngonPower_; return true; }
    if (name == "ngon_circle") { value = ngonCircle_; return true; }
    if (name == "ngon_corners") { value = ngonCorners_; return true; }
    return false;
}

bool VarNGon::setVariable(const std::string& name, double& value) {
    if (name == "ngon_sides") {
        // Nudges values in (-1, 1) away from 0 before rounding, so
        // ngon_sides can never become 0 (which would divide by zero
        // computing csides_ in prepare()).
        if (value < 0 && value > -1) value = -1;
        else if (value >= 0 && value < 1) value = 1;
        ngonSides_ = static_cast<int>(std::lround(value));
        value = ngonSides_;
        return true;
    }
    if (name == "ngon_power") { ngonPower_ = value; return true; }
    if (name == "ngon_circle") { ngonCircle_ = value; return true; }
    if (name == "ngon_corners") { ngonCorners_ = value; return true; }
    return false;
}

bool VarNGon::resetVariable(const std::string& name) {
    if (name == "ngon_sides") { ngonSides_ = 4; return true; }
    if (name == "ngon_power") { ngonPower_ = 2; return true; }
    if (name == "ngon_circle") { ngonCircle_ = 1; return true; }
    if (name == "ngon_corners") { ngonCorners_ = 1; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarNGon>();
}

} // namespace apo
