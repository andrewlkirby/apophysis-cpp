#include "VarPostFalloff2.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Shared "falloff distance" computation used by all three calc variants.
// Named `computeFalloffDist`/`dist` throughout (not `d`) to avoid shadowing
// the inherited Variation::d (affine coefficient).
//
// Pascal: `if (invert <> 0) then d := 1 - d; if (d < 0) then d := 0;` is TWO
// separate if-statements on one line, not if/elseif - both conditions are
// checked independently regardless of each other. Preserved as written.
double computeFalloffDist(double dx, double dy, double dz, int invert, double mindist, double rmax) {
    double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (invert != 0) dist = 1 - dist;
    if (dist < 0) dist = 0;
    dist = (dist - mindist) * rmax;
    if (dist < 0) dist = 0;
    return dist;
}
} // namespace

void VarPostFalloff2::prepare() { rmax_ = 0.04 * scatter_; }

void VarPostFalloff2::calc() {
    const double inX = *px, inY = *py, inZ = *pz;
    const double dist = computeFalloffDist(inX - x0_, inY - y0_, inZ - z0_, invert_, mindist_, rmax_);

    *px = vvar * (inX + mulX_ * rng->uniform01() * dist);
    *py = vvar * (inY + mulY_ * rng->uniform01() * dist);
    *pz = vvar * (inZ + mulZ_ * rng->uniform01() * dist);
    // Delphi's Frac(x) == x - Trunc(x), which is exactly std::fmod(x, 1.0)
    // for any sign (both truncate toward zero) - an exact match, not an
    // approximation.
    *color = std::fabs(std::fmod(*color + mulC_ * rng->uniform01() * dist, 1.0));
}

void VarPostFalloff2::calcRadial() {
    const double inX = *px, inY = *py, inZ = *pz;
    const double rIn = std::sqrt(inX * inX + inY * inY + inZ * inZ) + 1e-6;
    const double dist = computeFalloffDist(inX - x0_, inY - y0_, inZ - z0_, invert_, mindist_, rmax_);

    const double sigma = std::asin(inZ / rIn) + mulZ_ * rng->uniform01() * dist;
    // ArcTan2(Y, X) matches C's atan2(y, x) argument order directly.
    const double phi = std::atan2(inY, inX) + mulY_ * rng->uniform01() * dist;
    const double r = rIn + mulX_ * rng->uniform01() * dist;

    const double sins = std::sin(sigma), coss = std::cos(sigma);
    const double sinp = std::sin(phi), cosp = std::cos(phi);

    *px = vvar * (r * coss * cosp);
    *py = vvar * (r * coss * sinp);
    *pz = vvar * sins;
    *color = std::fabs(std::fmod(*color + mulC_ * rng->uniform01() * dist, 1.0));
}

void VarPostFalloff2::calcGaussian() {
    const double inX = *px, inY = *py, inZ = *pz;
    const double dist = computeFalloffDist(inX - x0_, inY - y0_, inZ - z0_, invert_, mindist_, rmax_);

    const double sigma = dist * rng->uniform01() * 2 * kPi;
    const double phi = dist * rng->uniform01() * kPi;
    const double r = dist * rng->uniform01();

    const double sins = std::sin(sigma), coss = std::cos(sigma);
    const double sinp = std::sin(phi), cosp = std::cos(phi);

    *px = vvar * (inX + mulX_ * r * coss * cosp);
    *py = vvar * (inY + mulY_ * r * coss * sinp);
    *pz = vvar * (inZ + mulZ_ * r * sins);
    *color = std::fabs(std::fmod(*color + mulC_ * rng->uniform01() * dist, 1.0));
}

CalcFn VarPostFalloff2::selectCalcFunction() {
    if (blurtype_ == 1) return bindCalc<VarPostFalloff2, &VarPostFalloff2::calcRadial>(this);
    if (blurtype_ == 2) return bindCalc<VarPostFalloff2, &VarPostFalloff2::calcGaussian>(this);
    return bindCalc<VarPostFalloff2, &VarPostFalloff2::calc>(this);
}

std::string VarPostFalloff2::variableNameAt(int index) const {
    switch (index) {
        case 0: return "post_falloff2_scatter";
        case 1: return "post_falloff2_mindist";
        case 2: return "post_falloff2_mul_x";
        case 3: return "post_falloff2_mul_y";
        case 4: return "post_falloff2_mul_z";
        case 5: return "post_falloff2_mul_c";
        case 6: return "post_falloff2_x0";
        case 7: return "post_falloff2_y0";
        case 8: return "post_falloff2_z0";
        case 9: return "post_falloff2_invert";
        case 10: return "post_falloff2_type"; // note: not "..._blurtype", matches source exactly
        default: return "";
    }
}

bool VarPostFalloff2::getVariable(const std::string& name, double& value) const {
    if (name == "post_falloff2_scatter") { value = scatter_; return true; }
    if (name == "post_falloff2_mindist") { value = mindist_; return true; }
    if (name == "post_falloff2_mul_x") { value = mulX_; return true; }
    if (name == "post_falloff2_mul_y") { value = mulY_; return true; }
    if (name == "post_falloff2_mul_z") { value = mulZ_; return true; }
    if (name == "post_falloff2_mul_c") { value = mulC_; return true; }
    if (name == "post_falloff2_x0") { value = x0_; return true; }
    if (name == "post_falloff2_y0") { value = y0_; return true; }
    if (name == "post_falloff2_z0") { value = z0_; return true; }
    if (name == "post_falloff2_invert") { value = invert_; return true; }
    if (name == "post_falloff2_type") { value = blurtype_; return true; }
    return false;
}

bool VarPostFalloff2::setVariable(const std::string& name, double& value) {
    if (name == "post_falloff2_scatter") {
        if (value < 1e-6) value = 1e-6;
        scatter_ = value;
        return true;
    }
    if (name == "post_falloff2_mindist") {
        if (value < 0) value = 0;
        mindist_ = value;
        return true;
    }
    if (name == "post_falloff2_mul_x") {
        if (value < 0) value = 0;
        else if (value > 1) value = 1;
        mulX_ = value;
        return true;
    }
    if (name == "post_falloff2_mul_y") {
        if (value < 0) value = 0;
        else if (value > 1) value = 1;
        mulY_ = value;
        return true;
    }
    if (name == "post_falloff2_mul_z") {
        if (value < 0) value = 0;
        else if (value > 1) value = 1;
        mulZ_ = value;
        return true;
    }
    if (name == "post_falloff2_mul_c") {
        if (value < 0) value = 0;
        else if (value > 1) value = 1;
        mulC_ = value;
        return true;
    }
    if (name == "post_falloff2_x0") { x0_ = value; return true; }
    if (name == "post_falloff2_y0") { y0_ = value; return true; }
    if (name == "post_falloff2_z0") { z0_ = value; return true; }
    if (name == "post_falloff2_invert") {
        if (value > 1) value = 1;
        if (value < 0) value = 0;
        invert_ = static_cast<int>(std::lround(value));
        return true;
    }
    if (name == "post_falloff2_type") {
        if (value > 2) value = 2;
        if (value < 0) value = 0;
        blurtype_ = static_cast<int>(std::lround(value));
        return true;
    }
    return false;
}

bool VarPostFalloff2::resetVariable(const std::string& name) {
    if (name == "post_falloff2_scatter") { scatter_ = 1; return true; }
    if (name == "post_falloff2_mindist") { mindist_ = 0.5; return true; }
    if (name == "post_falloff2_mul_x") { mulX_ = 1; return true; }
    if (name == "post_falloff2_mul_y") { mulY_ = 1; return true; }
    if (name == "post_falloff2_mul_z") { mulZ_ = 0; return true; }
    if (name == "post_falloff2_mul_c") { mulC_ = 0; return true; }
    if (name == "post_falloff2_x0") { x0_ = 0; return true; }
    if (name == "post_falloff2_y0") { y0_ = 0; return true; }
    if (name == "post_falloff2_z0") { z0_ = 0; return true; }
    if (name == "post_falloff2_invert") { invert_ = 0; return true; }
    if (name == "post_falloff2_type") { blurtype_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarPostFalloff2>();
}

} // namespace apo
