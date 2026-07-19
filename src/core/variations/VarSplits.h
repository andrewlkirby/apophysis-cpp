#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varSplits.pas (TVariationSplits).
class VarSplits final : public Variation {
public:
    static constexpr const char* kName = "splits";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 2; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;

private:
    double splitsX_ = 0;
    double splitsY_ = 0;
};

} // namespace apo
