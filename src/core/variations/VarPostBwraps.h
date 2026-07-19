#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varPostBwraps.pas (TVariationPostBwraps). A "post"
// variation: reads the already-accumulated *px/*py (not *tx/*ty) and
// conditionally REPLACES *px/*py/*pz (assignment, not accumulation) only
// when the point falls within the wrap cell's radius - if it doesn't (or
// cellsize is 0), *px/*py/*pz are left completely untouched, matching the
// original's `if post_bwraps_cellsize <> 0.0 ... if (...) <= r2 ...` guard
// structure exactly (there is no code path that zeroes or adds to the
// accumulator on the "outside" branch - it's simply skipped).
class VarPostBwraps final : public Variation {
public:
    static constexpr const char* kName = "post_bwraps";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 5; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double cellsize_ = 1;
    double space_ = 0;
    double gain_ = 1;
    double innerTwist_ = 0;
    double outerTwist_ = 0;

    // Precomputed in prepare().
    double g2_ = 0, r2_ = 0, rfactor_ = 0;
};

} // namespace apo
