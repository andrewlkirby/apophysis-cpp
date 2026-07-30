#pragma once

#include "../Variation.h"

namespace apo {

// Not present in Apophysis 7X's Pascal source - ported from flam3's
// variations.c (var30_perspective). flam3's own xform-init path zeroes
// perspective_angle/perspective_dist uniformly along with every other
// variation's parameters before any variation is actually added to an
// xform - not a meaningful default (angle=dist=0 makes the projection
// denominator identically zero). angle=0.5, dist=2.0 chosen instead as a
// deliberately non-degenerate starting point (a moderate ~45-degree tilt
// at a comfortable projection distance).
class VarPerspective final : public Variation {
public:
    static constexpr const char* kName = "perspective";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 2; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;

private:
    double perspectiveAngle_ = 0.5;
    double perspectiveDist_ = 2.0;

    // Precomputed in prepare() from perspectiveAngle_/perspectiveDist_,
    // matching flam3's perspective_precalc.
    double perspVsin_ = 0, perspVfcos_ = 0;
};

} // namespace apo
