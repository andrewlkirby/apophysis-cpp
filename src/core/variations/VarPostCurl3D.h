#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varPostCurl3D.pas (TVariationPostCurl3D). A "post"
// variation: reads and always ASSIGNS to *px/*py/*pz (all three, unlike
// VarPostCurl/VarPostCrop which leave *pz untouched). Clamps its inputs to
// +-1e100 first, matching the original's own comment ("got weird FP
// overflow there...").
//
// Unlike VarPostCurl, prepare() here computes into separate cached fields
// (cxScaled_/cyScaled_/czScaled_, matching the Pascal's `_cx`/`_cy`/`_cz`
// locals) rather than mutating cx_/cy_/cz_ in place - no compounding bug.
class VarPostCurl3D final : public Variation {
public:
    static constexpr const char* kName = "post_curl3D";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 3; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double cx_ = 0, cy_ = 0, cz_ = 0;

    double cxScaled_ = 0, cyScaled_ = 0, czScaled_ = 0;
    double cx2_ = 0, cy2_ = 0, cz2_ = 0, cSum2_ = 0;
    double c2x_ = 0, c2y_ = 0, c2z_ = 0;
};

} // namespace apo
