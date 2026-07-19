#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varEpispiral.pas (TVariationEpispiral).
//
// calc() never writes to *pz at all (every other variation in this
// codebase adds `vvar * *tz`; this one simply doesn't), so epispiral
// contributes nothing to the z coordinate - preserved as-is.
//
// Fixed bug: the original's ResetVariable defaults epispiral_holes to 0.0,
// while the constructor's default is 1.0 - reset now matches the
// constructor default instead of leaving that inconsistency in place.
class VarEpispiral final : public Variation {
public:
    static constexpr const char* kName = "epispiral";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 3; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double n_ = 6.0;
    double thickness_ = 0.0;
    double holes_ = 1.0;
};

} // namespace apo
