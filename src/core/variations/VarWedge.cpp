#include "VarWedge.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kOneOverTwoPi = 0.15915494309189533576888376337251;
} // namespace

// Matches `wedge_angle := PI / 2.0;` in the original's Create.
VarWedge::VarWedge() : angle_(kPi / 2.0) {}

void VarWedge::prepare() { compFac_ = 1.0 - angle_ * count_ * kOneOverTwoPi; }

void VarWedge::calc() {
    double r = std::sqrt((*tx) * (*tx) + (*ty) * (*ty));
    double ang = std::atan2(*ty, *tx) + swirl_ * r;
    const int cell = static_cast<int>(std::floor((count_ * ang + kPi) * kOneOverTwoPi));
    ang = ang * compFac_ + cell * angle_;
    const double sina = std::sin(ang), cosa = std::cos(ang);

    r = vvar * (r + hole_);
    *px += r * cosa;
    *py += r * sina;
    *pz += vvar * *tz;
}

std::string VarWedge::variableNameAt(int index) const {
    switch (index) {
        case 0: return "wedge_angle";
        case 1: return "wedge_hole";
        case 2: return "wedge_count";
        case 3: return "wedge_swirl";
        default: return "";
    }
}

bool VarWedge::getVariable(const std::string& name, double& value) const {
    if (name == "wedge_angle") { value = angle_; return true; }
    if (name == "wedge_hole") { value = hole_; return true; }
    if (name == "wedge_count") { value = count_; return true; }
    if (name == "wedge_swirl") { value = swirl_; return true; }
    return false;
}

bool VarWedge::setVariable(const std::string& name, double& value) {
    if (name == "wedge_angle") { angle_ = value; return true; }
    if (name == "wedge_hole") { hole_ = value; return true; }
    if (name == "wedge_count") {
        // Matches the original's clamp-then-double-round: values below 1
        // are clamped to 1, then rounded (the original rounds twice - once
        // into `Value`, once again into `wedge_count` - which is a no-op
        // the second time since Value is already integer-valued by then;
        // preserved here as a single round with the same net effect).
        if (value < 1) value = 1;
        count_ = static_cast<int>(std::lround(value));
        value = count_;
        return true;
    }
    if (name == "wedge_swirl") { swirl_ = value; return true; }
    return false;
}

bool VarWedge::resetVariable(const std::string& name) {
    if (name == "wedge_angle") { angle_ = kPi / 2; return true; }
    if (name == "wedge_hole") { hole_ = 0; return true; }
    if (name == "wedge_count") { count_ = 2; return true; }
    if (name == "wedge_swirl") { swirl_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarWedge>();
}

} // namespace apo
