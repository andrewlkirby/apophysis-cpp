#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varJulia3Dz.pas (TVariationJulia3D, registered
// name "julia3Dz"). Similar structure to VarJulia3Djf but with different
// formulas throughout (different cN, different r2d meaning in the fast
// paths, different sign/angle handling in CalcPowerMinus2, and
// CalcPowerMinus1 has no z-term in its denominator, unlike julia3Djf's) -
// ported directly against this file's own source, not by analogy.
class VarJulia3Dz final : public Variation {
public:
    static constexpr const char* kName = "julia3Dz";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    VarJulia3Dz();

    void prepare() override;
    void calc() override;
    CalcFn selectCalcFunction() override;

    int numVariables() const override { return 1; }
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

    // Precomputed in prepare() from n_.
    int absN_ = 0;
    double cN_ = 0;
};

} // namespace apo
