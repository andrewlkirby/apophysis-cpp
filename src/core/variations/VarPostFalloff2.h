#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varPostFalloff2.pas (TVariationPostFalloff2).
// Overrides selectCalcFunction() since GetCalcFunction genuinely dispatches
// on the current post_falloff2_type (blurtype) parameter value (0/1/2),
// re-evaluated every XForm::prepare() - unlike curl3D's version, this
// dispatch is real, not dead code.
//
// Note calc()/calcRadial()/calcGaussian() OVERWRITE the output accumulator
// (*px = ..., not *px += ...): they read whatever earlier variations in the
// calc list already accumulated (the 'post_' name prefix means XForm's
// dispatch always runs this last) as their own input, then replace it
// entirely rather than adding to it.
class VarPostFalloff2 final : public Variation {
public:
    static constexpr const char* kName = "post_falloff2";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;
    void calcRadial();
    void calcGaussian();
    CalcFn selectCalcFunction() override;

    int numVariables() const override { return 11; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double rmax_ = 0;
    double x0_ = 0, y0_ = 0, z0_ = 0;
    double scatter_ = 1, mindist_ = 0.5;
    int invert_ = 0, blurtype_ = 0;
    double mulX_ = 1, mulY_ = 1, mulZ_ = 0, mulC_ = 0;
};

} // namespace apo
