#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varWaves2.pas (TVariationWaves2).
class VarWaves2 final : public Variation {
public:
    static constexpr const char* kName = "waves2";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 6; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double freqX_ = 2, freqY_ = 2, freqZ_ = 0;
    double scaleX_ = 1, scaleY_ = 1, scaleZ_ = 0;
};

} // namespace apo
