#include "VarBipolar.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kHalfPi = 1.57079632679489661923;
constexpr double kPi = 3.14159265358979323846;
} // namespace

void VarBipolar::prepare() {
    v4_ = vvar * 0.15915494309189533576888376337251;
    v_ = vvar * 0.636619772367581343075535053490061;
    s_ = -kHalfPi * bipolarShift_;
}

void VarBipolar::calc() {
    const double x2y2 = (*tx) * (*tx) + (*ty) * (*ty);
    double y = 0.5 * std::atan2(2.0 * (*ty), x2y2 - 1.0) + s_;

    if (y > kHalfPi) {
        y = -kHalfPi + std::fmod(y + kHalfPi, kPi);
    } else if (y < -kHalfPi) {
        y = kHalfPi - std::fmod(kHalfPi - y, kPi);
    }

    const double t = x2y2 + 1.0;
    const double x2 = 2.0 * (*tx);

    const double numer = t + x2;
    const double denom = t - x2;

    // Matches the original's early `Exit`: on this condition, none of
    // FPx/FPy/FPz get touched at all for this point (the FPz accumulation
    // below is skipped too, not just FPx/FPy).
    if (denom == 0 || numer / denom <= 0) return;

    *px += v4_ * std::log(numer / denom);
    *py += v_ * y;
    *pz += vvar * *tz;
}

std::string VarBipolar::variableNameAt(int index) const {
    if (index == 0) return "bipolar_shift";
    return "";
}

bool VarBipolar::getVariable(const std::string& name, double& value) const {
    if (name == "bipolar_shift") { value = bipolarShift_; return true; }
    return false;
}

bool VarBipolar::setVariable(const std::string& name, double& value) {
    if (name == "bipolar_shift") {
        // Matches the original's normalize-into-[-1,1] behavior, including
        // writing the normalized result back through `value`.
        const double temp = std::fmod(0.5 * (value + 1.0), 1.0);
        value = 2.0 * temp - 1.0;
        bipolarShift_ = value;
        return true;
    }
    return false;
}

bool VarBipolar::resetVariable(const std::string& name) {
    if (name == "bipolar_shift") { bipolarShift_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarBipolar>();
}

} // namespace apo
