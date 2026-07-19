#include "VarPreBwraps.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarPreBwraps::prepare() {
    const double radius = 0.5 * (cellsize_ / (1.0 + space_ * space_));
    g2_ = gain_ * gain_ / (radius + 1e-6) + 1e-6;
    double maxBubble = g2_ * radius;

    if (maxBubble > 2.0) {
        maxBubble = 1.0;
    } else {
        maxBubble = maxBubble * (1.0 / (maxBubble * maxBubble / 4.0 + 1.0));
    }

    r2_ = radius * radius;
    rfactor_ = radius / maxBubble;
}

void VarPreBwraps::calc() {
    double vx = *tx;
    double vy = *ty;

    if (cellsize_ == 0.0) return; // matches the Pascal `if (cellsize <> 0.0)` guard - no-op otherwise

    const double cx = (std::floor(vx / cellsize_) + 0.5) * cellsize_;
    const double cy = (std::floor(vy / cellsize_) + 0.5) * cellsize_;

    double lx = vx - cx;
    double ly = vy - cy;

    if (lx * lx + ly * ly > r2_) return; // outside inner radius - also a no-op

    lx = lx * g2_;
    ly = ly * g2_;

    double r = rfactor_ / ((lx * lx + ly * ly) / 4.0 + 1);

    lx = lx * r;
    ly = ly * r;

    r = (lx * lx + ly * ly) / r2_;
    const double theta = innerTwist_ * (1.0 - r) + outerTwist_ * r;
    const double sinTheta = std::sin(theta), cosTheta = std::cos(theta);

    vx = cx + cosTheta * lx + sinTheta * ly;
    vy = cy - sinTheta * lx + cosTheta * ly;

    *tx = vvar * vx;
    *ty = vvar * vy;
    *tz = vvar * *tz;
}

std::string VarPreBwraps::variableNameAt(int index) const {
    switch (index) {
        case 0: return "pre_bwraps_cellsize";
        case 1: return "pre_bwraps_space";
        case 2: return "pre_bwraps_gain";
        case 3: return "pre_bwraps_inner_twist";
        case 4: return "pre_bwraps_outer_twist";
        default: return "";
    }
}

bool VarPreBwraps::getVariable(const std::string& name, double& value) const {
    // Pascal's cellsize getter has a dead `if Value = 0 then Value := 1e-6;`
    // line right before Value is unconditionally overwritten with the
    // stored field anyway - the caller-supplied `value` never survives, so
    // that branch has zero observable effect. Omitted here since it does
    // nothing.
    if (name == "pre_bwraps_cellsize") { value = cellsize_; return true; }
    if (name == "pre_bwraps_space") { value = space_; return true; }
    if (name == "pre_bwraps_gain") { value = gain_; return true; }
    if (name == "pre_bwraps_inner_twist") { value = innerTwist_; return true; }
    if (name == "pre_bwraps_outer_twist") { value = outerTwist_; return true; }
    return false;
}

bool VarPreBwraps::setVariable(const std::string& name, double& value) {
    if (name == "pre_bwraps_cellsize") { cellsize_ = value; return true; }
    if (name == "pre_bwraps_space") { space_ = value; return true; }
    if (name == "pre_bwraps_gain") { gain_ = value; return true; }
    if (name == "pre_bwraps_inner_twist") { innerTwist_ = value; return true; }
    if (name == "pre_bwraps_outer_twist") { outerTwist_ = value; return true; }
    return false;
}

bool VarPreBwraps::resetVariable(const std::string& name) {
    if (name == "pre_bwraps_cellsize") { cellsize_ = 1; return true; }
    if (name == "pre_bwraps_space") { space_ = 0; return true; }
    if (name == "pre_bwraps_gain") { gain_ = 1; return true; }
    if (name == "pre_bwraps_inner_twist") { innerTwist_ = 0; return true; }
    if (name == "pre_bwraps_outer_twist") { outerTwist_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarPreBwraps>();
}

} // namespace apo
