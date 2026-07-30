#include "VarPie.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

void VarPie::calc() {
    const int slice = static_cast<int>(rng->uniform01() * pieSlices_ + 0.5);
    const double angle =
        pieRotation_ + kTwoPi * (slice + rng->uniform01() * pieThickness_) / pieSlices_;
    const double r = vvar * rng->uniform01();

    *px += r * std::cos(angle);
    *py += r * std::sin(angle);
    *pz += vvar * *tz;
}

std::string VarPie::variableNameAt(int index) const {
    switch (index) {
        case 0: return "pie_slices";
        case 1: return "pie_rotation";
        case 2: return "pie_thickness";
        default: return "";
    }
}

bool VarPie::getVariable(const std::string& name, double& value) const {
    if (name == "pie_slices") { value = pieSlices_; return true; }
    if (name == "pie_rotation") { value = pieRotation_; return true; }
    if (name == "pie_thickness") { value = pieThickness_; return true; }
    return false;
}

bool VarPie::setVariable(const std::string& name, double& value) {
    if (name == "pie_slices") { pieSlices_ = value; return true; }
    if (name == "pie_rotation") { pieRotation_ = value; return true; }
    if (name == "pie_thickness") { pieThickness_ = value; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarPie>();
}

} // namespace apo
