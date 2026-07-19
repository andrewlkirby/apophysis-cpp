#include "VarPreFalloff2.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;

double computeFalloffDist(double dx, double dy, double dz, int invert, double mindist, double rmax) {
    double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (invert != 0) dist = 1 - dist;
    if (dist < 0) dist = 0;
    dist = (dist - mindist) * rmax;
    if (dist < 0) dist = 0;
    return dist;
}
} // namespace

void VarPreFalloff2::prepare() { rmax_ = 0.04 * scatter_; }

void VarPreFalloff2::calc() {
    const double inX = *tx, inY = *ty, inZ = *tz;
    const double dist = computeFalloffDist(inX - x0_, inY - y0_, inZ - z0_, invert_, mindist_, rmax_);

    *tx = vvar * (inX + mulX_ * rng->uniform01() * dist);
    *ty = vvar * (inY + mulY_ * rng->uniform01() * dist);
    *tz = vvar * (inZ + mulZ_ * rng->uniform01() * dist);
    *color = std::fabs(std::fmod(*color + mulC_ * rng->uniform01() * dist, 1.0));
}

void VarPreFalloff2::calcRadial() {
    const double inX = *tx, inY = *ty, inZ = *tz;
    const double rIn = std::sqrt(inX * inX + inY * inY + inZ * inZ) + 1e-6;
    const double dist = computeFalloffDist(inX - x0_, inY - y0_, inZ - z0_, invert_, mindist_, rmax_);

    const double sigma = std::asin(inZ / rIn) + mulZ_ * rng->uniform01() * dist;
    const double phi = std::atan2(inY, inX) + mulY_ * rng->uniform01() * dist;
    const double r = rIn + mulX_ * rng->uniform01() * dist;

    const double sins = std::sin(sigma), coss = std::cos(sigma);
    const double sinp = std::sin(phi), cosp = std::cos(phi);

    *tx = vvar * (r * coss * cosp);
    *ty = vvar * (r * coss * sinp);
    *tz = vvar * sins;
    *color = std::fabs(std::fmod(*color + mulC_ * rng->uniform01() * dist, 1.0));
}

void VarPreFalloff2::calcGaussian() {
    const double inX = *tx, inY = *ty, inZ = *tz;
    const double dist = computeFalloffDist(inX - x0_, inY - y0_, inZ - z0_, invert_, mindist_, rmax_);

    const double sigma = dist * rng->uniform01() * 2 * kPi;
    const double phi = dist * rng->uniform01() * kPi;
    const double r = dist * rng->uniform01();

    const double sins = std::sin(sigma), coss = std::cos(sigma);
    const double sinp = std::sin(phi), cosp = std::cos(phi);

    *tx = vvar * (inX + mulX_ * r * coss * cosp);
    *ty = vvar * (inY + mulY_ * r * coss * sinp);
    *tz = vvar * (inZ + mulZ_ * r * sins);
    *color = std::fabs(std::fmod(*color + mulC_ * rng->uniform01() * dist, 1.0));
}

CalcFn VarPreFalloff2::selectCalcFunction() {
    if (blurtype_ == 1) return bindCalc<VarPreFalloff2, &VarPreFalloff2::calcRadial>(this);
    if (blurtype_ == 2) return bindCalc<VarPreFalloff2, &VarPreFalloff2::calcGaussian>(this);
    return bindCalc<VarPreFalloff2, &VarPreFalloff2::calc>(this);
}

std::string VarPreFalloff2::variableNameAt(int index) const {
    switch (index) {
        case 0: return "pre_falloff2_scatter";
        case 1: return "pre_falloff2_mindist";
        case 2: return "pre_falloff2_mul_x";
        case 3: return "pre_falloff2_mul_y";
        case 4: return "pre_falloff2_mul_z";
        case 5: return "pre_falloff2_mul_c";
        case 6: return "pre_falloff2_x0";
        case 7: return "pre_falloff2_y0";
        case 8: return "pre_falloff2_z0";
        case 9: return "pre_falloff2_invert";
        case 10: return "pre_falloff2_type";
        default: return "";
    }
}

bool VarPreFalloff2::getVariable(const std::string& name, double& value) const {
    if (name == "pre_falloff2_scatter") { value = scatter_; return true; }
    if (name == "pre_falloff2_mindist") { value = mindist_; return true; }
    if (name == "pre_falloff2_mul_x") { value = mulX_; return true; }
    if (name == "pre_falloff2_mul_y") { value = mulY_; return true; }
    if (name == "pre_falloff2_mul_z") { value = mulZ_; return true; }
    if (name == "pre_falloff2_mul_c") { value = mulC_; return true; }
    if (name == "pre_falloff2_x0") { value = x0_; return true; }
    if (name == "pre_falloff2_y0") { value = y0_; return true; }
    if (name == "pre_falloff2_z0") { value = z0_; return true; }
    if (name == "pre_falloff2_invert") { value = invert_; return true; }
    if (name == "pre_falloff2_type") { value = blurtype_; return true; }
    return false;
}

bool VarPreFalloff2::setVariable(const std::string& name, double& value) {
    if (name == "pre_falloff2_scatter") {
        if (value < 1e-6) value = 1e-6;
        scatter_ = value;
        return true;
    }
    if (name == "pre_falloff2_mindist") {
        if (value < 0) value = 0;
        mindist_ = value;
        return true;
    }
    if (name == "pre_falloff2_mul_x") {
        if (value < 0) value = 0;
        else if (value > 1) value = 1;
        mulX_ = value;
        return true;
    }
    if (name == "pre_falloff2_mul_y") {
        if (value < 0) value = 0;
        else if (value > 1) value = 1;
        mulY_ = value;
        return true;
    }
    if (name == "pre_falloff2_mul_z") {
        if (value < 0) value = 0;
        else if (value > 1) value = 1;
        mulZ_ = value;
        return true;
    }
    if (name == "pre_falloff2_mul_c") {
        if (value < 0) value = 0;
        else if (value > 1) value = 1;
        mulC_ = value;
        return true;
    }
    if (name == "pre_falloff2_x0") { x0_ = value; return true; }
    if (name == "pre_falloff2_y0") { y0_ = value; return true; }
    if (name == "pre_falloff2_z0") { z0_ = value; return true; }
    if (name == "pre_falloff2_invert") {
        if (value > 1) value = 1;
        if (value < 0) value = 0;
        invert_ = static_cast<int>(std::lround(value));
        return true;
    }
    if (name == "pre_falloff2_type") {
        if (value > 2) value = 2;
        if (value < 0) value = 0;
        blurtype_ = static_cast<int>(std::lround(value));
        return true;
    }
    return false;
}

bool VarPreFalloff2::resetVariable(const std::string& name) {
    if (name == "pre_falloff2_scatter") { scatter_ = 1; return true; }
    if (name == "pre_falloff2_mindist") { mindist_ = 0.5; return true; }
    if (name == "pre_falloff2_mul_x") { mulX_ = 1; return true; }
    if (name == "pre_falloff2_mul_y") { mulY_ = 1; return true; }
    if (name == "pre_falloff2_mul_z") { mulZ_ = 0; return true; }
    if (name == "pre_falloff2_mul_c") { mulC_ = 0; return true; }
    if (name == "pre_falloff2_x0") { x0_ = 0; return true; }
    if (name == "pre_falloff2_y0") { y0_ = 0; return true; }
    if (name == "pre_falloff2_z0") { z0_ = 0; return true; }
    if (name == "pre_falloff2_invert") { invert_ = 0; return true; }
    if (name == "pre_falloff2_type") { blurtype_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarPreFalloff2>();
}

} // namespace apo
