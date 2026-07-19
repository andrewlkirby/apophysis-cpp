#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varPreBwraps.pas (TVariationPreBwraps).
//
// calc() is a genuine conditional no-op: if pre_bwraps_cellsize == 0, or
// the point falls outside the inner-cell radius, *tx/*ty/*tz are left
// completely untouched (no fallback identity*vvar write) - not scaled by
// vvar at all in that case. Preserved exactly as written; do not add a
// fallback path.
class VarPreBwraps final : public Variation {
public:
    static constexpr const char* kName = "pre_bwraps";
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
    double cellsize_ = 1, space_ = 0, gain_ = 1;
    double innerTwist_ = 0, outerTwist_ = 0;

    // Precomputed in prepare().
    double g2_ = 0, r2_ = 0, rfactor_ = 0;
};

} // namespace apo
