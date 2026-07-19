#include "VarLazysusan.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

void VarLazysusan::calc() {
    const double x = *tx - x_;
    const double y = *ty + y_;
    double r = std::sqrt(x * x + y * y);

    if (r < vvar) {
        const double angle = std::atan2(y, x) + spin_ + twist_ * (vvar - r);
        *px += vvar * (r * std::cos(angle) + x_);
        *py += vvar * (r * std::sin(angle) - y_);
    } else {
        r = 1.0 + space_ / (r + 1e-6);
        *px += vvar * (r * x + x_);
        *py += vvar * (r * y - y_);
    }

    *pz += vvar * *tz;
}

std::string VarLazysusan::variableNameAt(int index) const {
    switch (index) {
        case 0: return "lazysusan_spin";
        case 1: return "lazysusan_space";
        case 2: return "lazysusan_twist";
        case 3: return "lazysusan_x";
        case 4: return "lazysusan_y";
        default: return "";
    }
}

bool VarLazysusan::getVariable(const std::string& name, double& value) const {
    if (name == "lazysusan_spin") { value = spin_; return true; }
    if (name == "lazysusan_space") { value = space_; return true; }
    if (name == "lazysusan_twist") { value = twist_; return true; }
    if (name == "lazysusan_x") { value = x_; return true; }
    if (name == "lazysusan_y") { value = y_; return true; }
    return false;
}

bool VarLazysusan::setVariable(const std::string& name, double& value) {
    if (name == "lazysusan_spin") {
        // Pascal: `Value := frac(value / (2*PI)) * (2*PI); lazysusan_spin :=
        // value;` - `Value` and `value` are the SAME identifier in Pascal
        // (case-insensitive), so the wrapped result is both what's stored
        // AND what's reported back through the out-param, not the raw
        // input. frac() truncates toward zero (like std::fmod), so this is
        // not a clean [0, 2pi) mod for negative inputs - preserved exactly.
        value = std::fmod(value / kTwoPi, 1.0) * kTwoPi;
        spin_ = value;
        return true;
    }
    if (name == "lazysusan_space") { space_ = value; return true; }
    if (name == "lazysusan_twist") { twist_ = value; return true; }
    if (name == "lazysusan_x") { x_ = value; return true; }
    if (name == "lazysusan_y") { y_ = value; return true; }
    return false;
}

bool VarLazysusan::resetVariable(const std::string& name) {
    if (name == "lazysusan_spin") { spin_ = 3.14159265358979323846; return true; }
    if (name == "lazysusan_space") { space_ = 0; return true; }
    if (name == "lazysusan_twist") { twist_ = 0; return true; }
    if (name == "lazysusan_x") { x_ = 0; return true; }
    if (name == "lazysusan_y") { y_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarLazysusan>();
}

} // namespace apo
