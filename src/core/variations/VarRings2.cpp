#include "VarRings2.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kEps = 1e-10;
}

// Matches `FVal := Random * 2;` in the original's Create.
VarRings2::VarRings2() : val_(constructionRandom01() * 2) {}

void VarRings2::prepare() { dx_ = val_ * val_ + kEps; }

void VarRings2::calc() {
    const double length = std::sqrt((*tx) * (*tx) + (*ty) * (*ty));
    // Delphi's `System.Int` truncates toward zero, matching std::trunc (not
    // std::floor). The original's own comments call this an intentionally
    // unrolled/precomputed version of a simpler formula, kept for
    // performance parity rather than re-simplified here.
    const double r = vvar * (2 - dx_ * (std::trunc((length / dx_ + 1) / 2) * 2 / length + 1));

    *px += r * *tx;
    *py += r * *ty;
    *pz += vvar * *tz;
}

std::string VarRings2::variableNameAt(int index) const { return index == 0 ? "rings2_val" : ""; }

bool VarRings2::getVariable(const std::string& name, double& value) const {
    if (name == "rings2_val") { value = val_; return true; }
    return false;
}

bool VarRings2::setVariable(const std::string& name, double& value) {
    if (name == "rings2_val") { val_ = value; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarRings2>();
}

} // namespace apo
