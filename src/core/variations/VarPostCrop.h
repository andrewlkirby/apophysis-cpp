#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varPostCrop.pas (TVariationPostCrop). A "post"
// variation: reads *px/*py (not *tx/*ty) and always ASSIGNS (not
// accumulates) to *px/*py - but never touches *pz at all (the original has
// no `FPz^ := ...` line anywhere in CalcFunction).
class VarPostCrop final : public Variation {
public:
    static constexpr const char* kName = "post_crop";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 6; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double x0_ = -1, y0_ = -1, x1_ = 1, y1_ = 1;
    double s_ = 0;
    int z_ = 0;

    // Precomputed in prepare(): x0_/x1_ and y0_/y1_ normalized so the first
    // of each pair is the smaller, plus scatter half-widths.
    double normX0_ = 0, normX1_ = 0, normY0_ = 0, normY1_ = 0;
    double w_ = 0, h_ = 0;
};

} // namespace apo
