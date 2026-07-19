#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varRadialBlur.pas (TVariationRadialBlur).
// Overrides selectCalcFunction() to pick calcZoom()/calcSpin() when
// spinVar_/zoomVar_ (both derived from `angle_` and vvar in prepare()) are
// ~zero - verified algebraically equivalent to the generic calc() in that
// limit (unlike VarCurl3D's dead-code specialization, this one is a real,
// correct fast path).
class VarRadialBlur final : public Variation {
public:
    static constexpr const char* kName = "radial_blur";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    VarRadialBlur();

    void prepare() override;
    void calc() override;
    CalcFn selectCalcFunction() override;

    int numVariables() const override { return 1; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    void calcZoom();
    void calcSpin();

    double angle_;
    double spinVar_ = 0;
    double zoomVar_ = 0;

    // gauss_rnd-style rolling buffer, refilled in prepare() and consumed
    // one slot per calc() call - matches the original's `rnd`/`N` fields.
    double rnd_[4] = {0, 0, 0, 0};
    int n_ = 0;
};

} // namespace apo
