#include "VarBwraps.h"

#include <cmath>

#include "../VariationRegistration.h"

namespace apo {

void VarBwraps::prepare() {
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

void VarBwraps::calc() {
    double vx = *tx;
    double vy = *ty;

    if (cellsize_ == 0.0) {
        *px += vvar * *tx;
        *py += vvar * *ty;
        *pz += vvar * *tz;
        return;
    }

    const double cx = (std::floor(vx / cellsize_) + 0.5) * cellsize_;
    const double cy = (std::floor(vy / cellsize_) + 0.5) * cellsize_;

    double lx = vx - cx;
    double ly = vy - cy;

    if (lx * lx + ly * ly > r2_) {
        *px += vvar * *tx;
        *py += vvar * *ty;
        *pz += vvar * *tz;
        return;
    }

    lx *= g2_;
    ly *= g2_;

    double r = rfactor_ / ((lx * lx + ly * ly) / 4.0 + 1);
    lx *= r;
    ly *= r;

    r = (lx * lx + ly * ly) / r2_;
    const double theta = innerTwist_ * (1.0 - r) + outerTwist_ * r;
    const double sinT = std::sin(theta);
    const double cosT = std::cos(theta);

    vx = cx + cosT * lx + sinT * ly;
    vy = cy - sinT * lx + cosT * ly;

    *px += vvar * vx;
    *py += vvar * vy;
    *pz += vvar * *tz;
}

std::string VarBwraps::variableNameAt(int index) const {
    switch (index) {
        case 0: return "bwraps_cellsize";
        case 1: return "bwraps_space";
        case 2: return "bwraps_gain";
        case 3: return "bwraps_inner_twist";
        case 4: return "bwraps_outer_twist";
        default: return "";
    }
}

bool VarBwraps::getVariable(const std::string& name, double& value) const {
    // Matches TVariationBwraps.GetVariable exactly, including a quirk in
    // the "bwraps_cellsize" branch: the Pascal source checks `if Value = 0
    // then Value := 1e-6` on the *incoming* value before unconditionally
    // overwriting Value with cellsize_ on the next line anyway - so that
    // check has no effect on what's actually returned. Not reproduced here
    // (there is nothing to reproduce: with `value` an out-parameter here,
    // not caller-initialized state, the equivalent check would be
    // meaningless in a different way) - the important behavior, returning
    // cellsize_ unconditionally, is preserved.
    if (name == "bwraps_cellsize") { value = cellsize_; return true; }
    if (name == "bwraps_space") { value = space_; return true; }
    if (name == "bwraps_gain") { value = gain_; return true; }
    if (name == "bwraps_inner_twist") { value = innerTwist_; return true; }
    if (name == "bwraps_outer_twist") { value = outerTwist_; return true; }
    return false;
}

bool VarBwraps::setVariable(const std::string& name, double& value) {
    if (name == "bwraps_cellsize") { cellsize_ = value; return true; }
    if (name == "bwraps_space") { space_ = value; return true; }
    if (name == "bwraps_gain") { gain_ = value; return true; }
    if (name == "bwraps_inner_twist") { innerTwist_ = value; return true; }
    if (name == "bwraps_outer_twist") { outerTwist_ = value; return true; }
    return false;
}

bool VarBwraps::resetVariable(const std::string& name) {
    if (name == "bwraps_cellsize") { cellsize_ = 1; return true; }
    if (name == "bwraps_space") { space_ = 0; return true; }
    if (name == "bwraps_gain") { gain_ = 1; return true; }
    if (name == "bwraps_inner_twist") { innerTwist_ = 0; return true; }
    if (name == "bwraps_outer_twist") { outerTwist_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarBwraps>();
}

} // namespace apo
