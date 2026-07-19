#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varJuliaN.pas (TVariationJulian). Overrides
// selectCalcFunction() to pick a cheaper specialized calc path when
// julian_dist == 1 and julian_power is +-1 or +-2 - re-evaluated every
// XForm::prepare() since those are just the variation's own current
// parameter values (see Variation::selectCalcFunction's doc comment).
class VarJulian final : public Variation {
public:
    static constexpr const char* kName = "julian";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    VarJulian();

    void prepare() override;
    void calc() override;
    CalcFn selectCalcFunction() override;

    int numVariables() const override { return 2; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    void calcPower1();
    void calcPowerMinus1();
    void calcPower2();
    void calcPowerMinus2();

    int n_;
    double c_ = 1.0;

    // Precomputed in prepare() from n_/c_/vvar.
    int absN_ = 0;
    double cN_ = 0;
    double vvar2_ = 0;
};

} // namespace apo
