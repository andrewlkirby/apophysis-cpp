#include "VarCurl3D.h"

#include "../VariationRegistration.h"

namespace apo {

void VarCurl3D::prepare() {
    c2x_ = 2 * cx_;
    c2y_ = 2 * cy_;
    c2z_ = 2 * cz_;

    cx2_ = cx_ * cx_;
    cy2_ = cy_ * cy_;
    cz2_ = cz_ * cz_;

    c2_ = cx2_ + cy2_ + cz2_;
}

void VarCurl3D::calc() {
    const double r2 = (*tx) * (*tx) + (*ty) * (*ty) + (*tz) * (*tz);
    const double r = vvar / (r2 * c2_ + c2x_ * (*tx) - c2y_ * (*ty) + c2z_ * (*tz) + 1);

    *px += r * (*tx + cx_ * r2);
    *py += r * (*ty - cy_ * r2);
    *pz += r * (*tz + cz_ * r2);
}

std::string VarCurl3D::variableNameAt(int index) const {
    switch (index) {
        case 0: return "curl3D_cx";
        case 1: return "curl3D_cy";
        case 2: return "curl3D_cz";
        default: return "";
    }
}

bool VarCurl3D::getVariable(const std::string& name, double& value) const {
    if (name == "curl3D_cx") { value = cx_; return true; }
    if (name == "curl3D_cy") { value = cy_; return true; }
    if (name == "curl3D_cz") { value = cz_; return true; }
    return false;
}

bool VarCurl3D::setVariable(const std::string& name, double& value) {
    if (name == "curl3D_cx") { cx_ = value; return true; }
    if (name == "curl3D_cy") { cy_ = value; return true; }
    if (name == "curl3D_cz") { cz_ = value; return true; }
    return false;
}

bool VarCurl3D::resetVariable(const std::string& name) {
    if (name == "curl3D_cx") { cx_ = 0; return true; }
    if (name == "curl3D_cy") { cy_ = 0; return true; }
    if (name == "curl3D_cz") { cz_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarCurl3D>();
}

} // namespace apo
