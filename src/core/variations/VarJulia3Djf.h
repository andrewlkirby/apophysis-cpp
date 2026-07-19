#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varJulia3Djf.pas (TVariationJulia3DJF, registered
// name "julia3D"). Not to be confused with VarJulian ("julian") or
// VarJulia3Dz ("julia3Dz") - three separate variations with similar-looking
// math but different exponents/formulas, ported independently against their
// own source files rather than by analogy to each other.
class VarJulia3Djf final : public Variation {
public:
    static constexpr const char* kName = "julia3D";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    VarJulia3Djf();

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
