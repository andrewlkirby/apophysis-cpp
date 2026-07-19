#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varSeparation.pas (TVariationSeparation).
class VarSeparation final : public Variation {
public:
    static constexpr const char* kName = "separation";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 4; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double separationX_ = 1;
    double separationY_ = 1;
    double separationXInside_ = 0;
    double separationYInside_ = 0;
};

} // namespace apo
