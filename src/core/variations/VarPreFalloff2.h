#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varPreFalloff2.pas (TVariationPreFalloff2).
// Structurally identical to VarPostFalloff2 except it reads/writes the
// INPUT point (*tx/*ty/*tz, via the 'pre_' name prefix) rather than the
// output accumulator - see VarPostFalloff2.h's comment for the
// selectCalcFunction() dispatch rationale, which applies identically here.
class VarPreFalloff2 final : public Variation {
public:
    static constexpr const char* kName = "pre_falloff2";
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
