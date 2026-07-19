#include "VarEpispiral.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarEpispiral::calc() {
    // Delphi: arctan2(FTy^, FTx^) - Y=FTy, X=FTx, same order as atan2(y, x).
    const double theta = std::atan2(*ty, *tx);

    const double t = (rng->uniform01() * thickness_) * (1 / std::cos(n_ * theta)) - holes_;

    // Exact-equality check in the original (`if (abs(t) = 0)`), not an
    // epsilon comparison - preserved as-is; in practice this branch is
    // essentially never taken since t is derived from a continuous random
    // value.
    if (std::fabs(t) == 0.0) {
        // No-op in the original (`FPx^ := FPx^; FPy^ := FPy^;`).
    } else {
        *px += vvar * t * std::cos(theta);
        *py += vvar * t * std::sin(theta);
    }
    // No *pz update anywhere in this variation - see the class comment.
}

std::string VarEpispiral::variableNameAt(int index) const {
    switch (index) {
        case 0: return "epispiral_n";
        case 1: return "epispiral_thickness";
        case 2: return "epispiral_holes";
        default: return "";
    }
}

bool VarEpispiral::getVariable(const std::string& name, double& value) const {
    if (name == "epispiral_n") { value = n_; return true; }
    if (name == "epispiral_thickness") { value = thickness_; return true; }
    if (name == "epispiral_holes") { value = holes_; return true; }
    return false;
}

bool VarEpispiral::setVariable(const std::string& name, double& value) {
    if (name == "epispiral_n") { n_ = value; return true; }
    if (name == "epispiral_thickness") { thickness_ = value; return true; }
    if (name == "epispiral_holes") { holes_ = value; return true; }
    return false;
}

bool VarEpispiral::resetVariable(const std::string& name) {
    if (name == "epispiral_n") { n_ = 6.0; return true; }
    if (name == "epispiral_thickness") { thickness_ = 0.0; return true; }
    if (name == "epispiral_holes") { holes_ = 1.0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarEpispiral>();
}

} // namespace apo
