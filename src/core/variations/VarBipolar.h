#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varBipolar.pas (TVariationBipolar).
class VarBipolar final : public Variation {
public:
    static constexpr const char* kName = "bipolar";
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
    double bipolarShift_ = 0;

    // Precomputed in prepare().
    double v4_ = 0, v_ = 0, s_ = 0;
};

} // namespace apo
