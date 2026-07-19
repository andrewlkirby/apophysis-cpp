#include "VarPostBwraps.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarPostBwraps::prepare() {
    const double radius = 0.5 * (cellsize_ / (1.0 + space_ * space_));
    g2_ = (gain_ * gain_) / (radius + 1e-6) + 1e-6;
    double maxBubble = g2_ * radius;

    if (maxBubble > 2.0) {
        maxBubble = 1.0;
    } else {
        maxBubble = maxBubble * (1.0 / (maxBubble * maxBubble / 4.0 + 1.0));
    }

    r2_ = radius * radius;
    rfactor_ = radius / maxBubble;
}

void VarPostBwraps::calc() {
    const double vx = *px;
    const double vy = *py;

    if (cellsize_ == 0.0) return;

    const double cx = (std::floor(vx / cellsize_) + 0.5) * cellsize_;
    const double cy = (std::floor(vy / cellsize_) + 0.5) * cellsize_;

    double lx = vx - cx;
    double ly = vy - cy;

    if (lx * lx + ly * ly > r2_) return; // outside the wrap cell radius: leave *px/*py/*pz untouched

    lx *= g2_;
    ly *= g2_;

    double r = rfactor_ / ((lx * lx + ly * ly) / 4.0 + 1);
    lx *= r;
    ly *= r;

    r = (lx * lx + ly * ly) / r2_;
    const double theta = innerTwist_ * (1.0 - r) + outerTwist_ * r;
    const double sinT = std::sin(theta);
    const double cosT = std::cos(theta);

    const double newVx = cx + cosT * lx + sinT * ly;
    const double newVy = cy - sinT * lx + cosT * ly;

    *px = vvar * newVx;
    *py = vvar * newVy;
    *pz = vvar * (*pz);
}

std::string VarPostBwraps::variableNameAt(int index) const {
    switch (index) {
        case 0: return "post_bwraps_cellsize";
        case 1: return "post_bwraps_space";
        case 2: return "post_bwraps_gain";
        case 3: return "post_bwraps_inner_twist";
        case 4: return "post_bwraps_outer_twist";
        default: return "";
    }
}

bool VarPostBwraps::getVariable(const std::string& name, double& value) const {
    if (name == "post_bwraps_cellsize") { value = cellsize_; return true; }
    if (name == "post_bwraps_space") { value = space_; return true; }
    if (name == "post_bwraps_gain") { value = gain_; return true; }
    if (name == "post_bwraps_inner_twist") { value = innerTwist_; return true; }
    if (name == "post_bwraps_outer_twist") { value = outerTwist_; return true; }
    return false;
}

bool VarPostBwraps::setVariable(const std::string& name, double& value) {
    if (name == "post_bwraps_cellsize") {
        // Matches the original: never let cellsize be stored as exactly 0
        // (which would permanently disable calc() via the `cellsize_ ==
        // 0.0` guard above).
        if (value == 0) value = 1e-6;
        cellsize_ = value;
        return true;
    }
    if (name == "post_bwraps_space") { space_ = value; return true; }
    if (name == "post_bwraps_gain") { gain_ = value; return true; }
    if (name == "post_bwraps_inner_twist") { innerTwist_ = value; return true; }
    if (name == "post_bwraps_outer_twist") { outerTwist_ = value; return true; }
    return false;
}

bool VarPostBwraps::resetVariable(const std::string& name) {
    if (name == "post_bwraps_cellsize") { cellsize_ = 1; return true; }
    if (name == "post_bwraps_space") { space_ = 0; return true; }
    if (name == "post_bwraps_gain") { gain_ = 1; return true; }
    if (name == "post_bwraps_inner_twist") { innerTwist_ = 0; return true; }
    if (name == "post_bwraps_outer_twist") { outerTwist_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarPostBwraps>();
}

} // namespace apo
