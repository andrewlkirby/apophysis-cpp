#include "VarPostCurl3D.h"

#include <algorithm>

#include "../VariationRegistration.h"

namespace apo {

void VarPostCurl3D::prepare() {
    cxScaled_ = vvar * cx_;
    cyScaled_ = vvar * cy_;
    czScaled_ = vvar * cz_;

    c2x_ = 2 * cxScaled_;
    c2y_ = 2 * cyScaled_;
    c2z_ = 2 * czScaled_;

    cx2_ = cxScaled_ * cxScaled_;
    cy2_ = cyScaled_ * cyScaled_;
    cz2_ = czScaled_ * czScaled_;

    cSum2_ = cx2_ + cy2_ + cz2_;
}

void VarPostCurl3D::calc() {
    const double x = std::max(-1e100, std::min(*px, 1e100));
    const double y = std::max(-1e100, std::min(*py, 1e100));
    const double z = std::max(-1e100, std::min(*pz, 1e100));

    const double r2 = x * x + y * y + z * z;
    const double r = 1.0 / (r2 * cSum2_ + c2x_ * x - c2y_ * y + c2z_ * z + 1);

    *px = r * (x + cxScaled_ * r2);
    *py = r * (y + cyScaled_ * r2);
    *pz = r * (z + czScaled_ * r2);
}

std::string VarPostCurl3D::variableNameAt(int index) const {
    switch (index) {
        case 0: return "post_curl3D_cx";
        case 1: return "post_curl3D_cy";
        case 2: return "post_curl3D_cz";
        default: return "";
    }
}

bool VarPostCurl3D::getVariable(const std::string& name, double& value) const {
    if (name == "post_curl3D_cx") { value = cx_; return true; }
    if (name == "post_curl3D_cy") { value = cy_; return true; }
    if (name == "post_curl3D_cz") { value = cz_; return true; }
    return false;
}

bool VarPostCurl3D::setVariable(const std::string& name, double& value) {
    if (name == "post_curl3D_cx") { cx_ = value; return true; }
    if (name == "post_curl3D_cy") { cy_ = value; return true; }
    if (name == "post_curl3D_cz") { cz_ = value; return true; }
    return false;
}

bool VarPostCurl3D::resetVariable(const std::string& name) {
    if (name == "post_curl3D_cx") { cx_ = 0; return true; }
    if (name == "post_curl3D_cy") { cy_ = 0; return true; }
    if (name == "post_curl3D_cz") { cz_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarPostCurl3D>();
}

} // namespace apo
