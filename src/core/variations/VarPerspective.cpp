#include "VarPerspective.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void VarPerspective::prepare() {
    const double ang = perspectiveAngle_ * kPi / 2.0;
    perspVsin_ = std::sin(ang);
    perspVfcos_ = perspectiveDist_ * std::cos(ang);
}

void VarPerspective::calc() {
    const double t = 1.0 / (perspectiveDist_ - *ty * perspVsin_);

    *px += vvar * perspectiveDist_ * *tx * t;
    *py += vvar * perspVfcos_ * *ty * t;
    *pz += vvar * *tz;
}

std::string VarPerspective::variableNameAt(int index) const {
    switch (index) {
        case 0: return "perspective_angle";
        case 1: return "perspective_dist";
        default: return "";
    }
}

bool VarPerspective::getVariable(const std::string& name, double& value) const {
    if (name == "perspective_angle") { value = perspectiveAngle_; return true; }
    if (name == "perspective_dist") { value = perspectiveDist_; return true; }
    return false;
}

bool VarPerspective::setVariable(const std::string& name, double& value) {
    if (name == "perspective_angle") { perspectiveAngle_ = value; return true; }
    if (name == "perspective_dist") { perspectiveDist_ = value; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarPerspective>();
}

} // namespace apo
