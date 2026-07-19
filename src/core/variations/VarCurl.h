#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varCurl.pas (TVariationCurl). Overrides
// selectCalcFunction() to dispatch to specialized branches when c1 and/or
// c2 are (approximately) zero - a genuine, live dispatch (unlike
// VarCurl3D's dead-code GetCalcFunction override, which never actually
// takes its specialized branches).
class VarCurl final : public Variation {
public:
    static constexpr const char* kName = "curl";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;
    CalcFn selectCalcFunction() override;

    int numVariables() const override { return 2; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    void calcZeroC2();
    void calcZeroC1();
    void calcZeroC2C1();

    double c1_ = 0, c2_ = 0;
    double c2x2_ = 0; // precomputed in prepare(): 2*c2_
};

} // namespace apo
