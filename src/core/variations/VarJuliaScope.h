#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varJuliaScope.pas (TVariationJuliaScope).
class VarJuliaScope final : public Variation {
public:
    static constexpr const char* kName = "juliascope";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    VarJuliaScope();

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
    double dist_ = 1.0;

    // Precomputed in prepare() from n_/dist_.
    int rN_ = 0;
    double cn_ = 0;
};

} // namespace apo
