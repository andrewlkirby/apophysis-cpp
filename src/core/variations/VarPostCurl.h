#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varPostCurl.pas (TVariationPostCurl). A "post"
// variation: reads *px/*py and always ASSIGNS (not accumulates); never
// touches *pz (no `FPz^ := ...` line in the original at all).
//
// Fixed bug: the original's Prepare does `c1 := c1 * VVAR; c2 := c2 * VVAR;`,
// mutating the STORED parameters in place instead of a separate cached
// value. Since XForm::prepare() runs on every render start (including
// repeated interactive-preview re-renders that reuse the same
// XForm/Variation instances), that made c1/c2 silently shrink/grow by a
// factor of vvar on every single prepare() call. Fixed here by deriving into
// separate cached locals (c1Scaled_/c2Scaled_/c22_), matching the correct
// pattern already used by the sibling VarPostCurl3D.
class VarPostCurl final : public Variation {
public:
    static constexpr const char* kName = "post_curl";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 2; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double c1_ = 0, c2_ = 0;
    double c1Scaled_ = 0, c2Scaled_ = 0, c22_ = 0;
};

} // namespace apo
