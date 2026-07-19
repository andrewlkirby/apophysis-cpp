#include "VarFalloff2.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarFalloff2::prepare() { rmax_ = 0.04 * scatter_; }

double VarFalloff2::falloffDistance(double inX, double inY, double inZ) const {
    double dist = std::sqrt((inX - x0_) * (inX - x0_) + (inY - y0_) * (inY - y0_) + (inZ - z0_) * (inZ - z0_));
    // Two independent statements in the original (`if invert...; if d<0...`),
    // not an if/else - both can fire in sequence.
    if (invert_ != 0) dist = 1 - dist;
    if (dist < 0) dist = 0;
    dist = (dist - mindist_) * rmax_;
    if (dist < 0) dist = 0;
    return dist;
}

void VarFalloff2::calc() {
    const double inX = *tx, inY = *ty, inZ = *tz;
    const double dist = falloffDistance(inX, inY, inZ);

    // Each `random` in the original is a fresh, independent call - not one
    // shared value reused across the four lines.
    *px += vvar * (inX + mulX_ * rng->uniform01() * dist);
    *py += vvar * (inY + mulY_ * rng->uniform01() * dist);
    *pz += vvar * (inZ + mulZ_ * rng->uniform01() * dist);
    const double colorArg = *color + mulC_ * rng->uniform01() * dist;
    *color = std::fabs(colorArg - std::trunc(colorArg));
}

void VarFalloff2::calcRadial() {
    const double inX = *tx, inY = *ty, inZ = *tz;
    const double rIn = std::sqrt(inX * inX + inY * inY + inZ * inZ) + 1e-6;
    const double dist = falloffDistance(inX, inY, inZ);

    const double sigma = std::asin(inZ / rIn) + mulZ_ * rng->uniform01() * dist;
    // Delphi: ArcTan2(in_y, in_x) - Y=in_y, X=in_x, same order as atan2(y,x).
    const double phi = std::atan2(inY, inX) + mulY_ * rng->uniform01() * dist;
    const double r = rIn + mulX_ * rng->uniform01() * dist;

    const double sins = std::sin(sigma), coss = std::cos(sigma);
    const double sinp = std::sin(phi), cosp = std::cos(phi);

    *px += vvar * (r * coss * cosp);
    *py += vvar * (r * coss * sinp);
    *pz += vvar * sins;
    const double colorArg = *color + mulC_ * rng->uniform01() * dist;
    *color = std::fabs(colorArg - std::trunc(colorArg));
}

void VarFalloff2::calcGaussian() {
    const double inX = *tx, inY = *ty, inZ = *tz;
    const double dist = falloffDistance(inX, inY, inZ);

    constexpr double kPi = 3.14159265358979323846;
    const double sigma = dist * rng->uniform01() * 2 * kPi;
    const double phi = dist * rng->uniform01() * kPi;
    const double r = dist * rng->uniform01();

    const double sins = std::sin(sigma), coss = std::cos(sigma);
    const double sinp = std::sin(phi), cosp = std::cos(phi);

    *px += vvar * (inX + mulX_ * r * coss * cosp);
    *py += vvar * (inY + mulY_ * r * coss * sinp);
    *pz += vvar * (inZ + mulZ_ * r * sins);
    const double colorArg = *color + mulC_ * rng->uniform01() * dist;
    *color = std::fabs(colorArg - std::trunc(colorArg));
}

CalcFn VarFalloff2::selectCalcFunction() {
    if (blurtype_ == 1) return bindCalc<VarFalloff2, &VarFalloff2::calcRadial>(this);
    if (blurtype_ == 2) return bindCalc<VarFalloff2, &VarFalloff2::calcGaussian>(this);
    return bindCalc<VarFalloff2, &VarFalloff2::calc>(this);
}

std::string VarFalloff2::variableNameAt(int index) const {
    switch (index) {
        case 0: return "falloff2_scatter";
        case 1: return "falloff2_mindist";
        case 2: return "falloff2_mul_x";
        case 3: return "falloff2_mul_y";
        case 4: return "falloff2_mul_z";
        case 5: return "falloff2_mul_c";
        case 6: return "falloff2_x0";
        case 7: return "falloff2_y0";
        case 8: return "falloff2_z0";
        case 9: return "falloff2_invert";
        case 10: return "falloff2_type";
        default: return "";
    }
}

bool VarFalloff2::getVariable(const std::string& name, double& value) const {
    if (name == "falloff2_scatter") { value = scatter_; return true; }
    if (name == "falloff2_mindist") { value = mindist_; return true; }
    if (name == "falloff2_mul_x") { value = mulX_; return true; }
    if (name == "falloff2_mul_y") { value = mulY_; return true; }
    if (name == "falloff2_mul_z") { value = mulZ_; return true; }
    if (name == "falloff2_mul_c") { value = mulC_; return true; }
    if (name == "falloff2_x0") { value = x0_; return true; }
    if (name == "falloff2_y0") { value = y0_; return true; }
    if (name == "falloff2_z0") { value = z0_; return true; }
    if (name == "falloff2_invert") { value = invert_; return true; }
    if (name == "falloff2_type") { value = blurtype_; return true; }
    return false;
}

bool VarFalloff2::setVariable(const std::string& name, double& value) {
    if (name == "falloff2_scatter") {
        if (value < 1e-6) value = 1e-6;
        scatter_ = value;
        return true;
    }
    if (name == "falloff2_mindist") {
        if (value < 0) value = 0;
        mindist_ = value;
        return true;
    }
    if (name == "falloff2_mul_x") {
        if (value < 0) value = 0; else if (value > 1) value = 1;
        mulX_ = value;
        return true;
    }
    if (name == "falloff2_mul_y") {
        if (value < 0) value = 0; else if (value > 1) value = 1;
        mulY_ = value;
        return true;
    }
    if (name == "falloff2_mul_z") {
        if (value < 0) value = 0; else if (value > 1) value = 1;
        mulZ_ = value;
        return true;
    }
    if (name == "falloff2_mul_c") {
        if (value < 0) value = 0; else if (value > 1) value = 1;
        mulC_ = value;
        return true;
    }
    if (name == "falloff2_x0") { x0_ = value; return true; }
    if (name == "falloff2_y0") { y0_ = value; return true; }
    if (name == "falloff2_z0") { z0_ = value; return true; }
    if (name == "falloff2_invert") {
        if (value > 1) value = 1;
        if (value < 0) value = 0;
        invert_ = static_cast<int>(std::lround(value));
        value = invert_;
        return true;
    }
    if (name == "falloff2_type") {
        if (value > 2) value = 2;
        if (value < 0) value = 0;
        blurtype_ = static_cast<int>(std::lround(value));
        value = blurtype_;
        return true;
    }
    return false;
}

bool VarFalloff2::resetVariable(const std::string& name) {
    if (name == "falloff2_scatter") { scatter_ = 1; return true; }
    if (name == "falloff2_mindist") { mindist_ = 0.5; return true; }
    if (name == "falloff2_mul_x") { mulX_ = 1; return true; }
    if (name == "falloff2_mul_y") { mulY_ = 1; return true; }
    if (name == "falloff2_mul_z") { mulZ_ = 0; return true; }
    if (name == "falloff2_mul_c") { mulC_ = 0; return true; }
    if (name == "falloff2_x0") { x0_ = 0; return true; }
    if (name == "falloff2_y0") { y0_ = 0; return true; }
    if (name == "falloff2_z0") { z0_ = 0; return true; }
    if (name == "falloff2_invert") { invert_ = 0; return true; }
    if (name == "falloff2_type") { blurtype_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarFalloff2>();
}

} // namespace apo
