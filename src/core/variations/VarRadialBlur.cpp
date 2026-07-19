#include "VarRadialBlur.h"

#include <cmath>

#include "../Rng.h"
#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-10;
bool isZero(double x) { return std::fabs(x) < kEps; }
} // namespace

// Matches `angle := random * 2 - 1;` in the original's Create.
VarRadialBlur::VarRadialBlur() : angle_(constructionRandom01() * 2 - 1) {}

void VarRadialBlur::prepare() {
    spinVar_ = vvar * std::sin(angle_ * kPi / 2);
    zoomVar_ = vvar * std::cos(angle_ * kPi / 2);

    n_ = 0;
    rnd_[0] = rng->uniform01();
    rnd_[1] = rng->uniform01();
    rnd_[2] = rng->uniform01();
    rnd_[3] = rng->uniform01();
}

void VarRadialBlur::calc() {
    const double rndG = rnd_[0] + rnd_[1] + rnd_[2] + rnd_[3] - 2;
    rnd_[n_] = rng->uniform01();
    n_ = (n_ + 1) & 3;

    const double ra = std::sqrt((*tx) * (*tx) + (*ty) * (*ty));
    const double angle = std::atan2(*ty, *tx) + spinVar_ * rndG;
    const double sina = std::sin(angle), cosa = std::cos(angle);
    const double rz = zoomVar_ * rndG - 1;

    *px += ra * cosa + rz * (*tx);
    *py += ra * sina + rz * (*ty);
    *pz += vvar * *tz;
}

void VarRadialBlur::calcZoom() {
    const double r = zoomVar_ * (rnd_[0] + rnd_[1] + rnd_[2] + rnd_[3] - 2);

    rnd_[n_] = rng->uniform01();
    n_ = (n_ + 1) & 3;

    *px += r * (*tx);
    *py += r * (*ty);
    *pz += vvar * *tz;
}

void VarRadialBlur::calcSpin() {
    const double angle = std::atan2(*ty, *tx) + spinVar_ * (rnd_[0] + rnd_[1] + rnd_[2] + rnd_[3] - 2);
    const double sina = std::sin(angle), cosa = std::cos(angle);
    const double r = std::sqrt((*tx) * (*tx) + (*ty) * (*ty));

    rnd_[n_] = rng->uniform01();
    n_ = (n_ + 1) & 3;

    *px += r * cosa - *tx;
    *py += r * sina - *ty;
    *pz += vvar * *tz;
}

CalcFn VarRadialBlur::selectCalcFunction() {
    if (isZero(spinVar_)) return bindCalc<VarRadialBlur, &VarRadialBlur::calcZoom>(this);
    if (isZero(zoomVar_)) return bindCalc<VarRadialBlur, &VarRadialBlur::calcSpin>(this);
    return bindCalc<VarRadialBlur, &VarRadialBlur::calc>(this);
}

std::string VarRadialBlur::variableNameAt(int index) const {
    return index == 0 ? "radial_blur_angle" : "";
}

bool VarRadialBlur::getVariable(const std::string& name, double& value) const {
    if (name == "radial_blur_angle") { value = angle_; return true; }
    return false;
}

bool VarRadialBlur::setVariable(const std::string& name, double& value) {
    if (name == "radial_blur_angle") { angle_ = value; return true; }
    return false;
}

bool VarRadialBlur::resetVariable(const std::string& name) {
    // Matches TVariationRadialBlur.ResetVariable's toggle-not-reset quirk:
    // it doesn't restore a fixed default, it flips between 0 and 1
    // (`if angle<>0 then angle:=0 else if angle=0 then angle:=1`) -
    // preserved exactly rather than "fixed" to a single sensible default.
    if (name == "radial_blur_angle") {
        angle_ = (angle_ != 0) ? 0 : 1;
        return true;
    }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarRadialBlur>();
}

} // namespace apo
