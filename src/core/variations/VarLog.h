#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varLog.pas (TVariationLog).
class VarLog final : public Variation {
public:
    static constexpr const char* kName = "log";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 1; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double base_ = 2.71828182845905;
    double denom_ = 0;
};

} // namespace apo
