#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varAuger.pas (TVariationAuger).
class VarAuger final : public Variation {
public:
    static constexpr const char* kName = "auger";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 4; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double augerFreq_ = 5;
    double augerWeight_ = 0.5;
    double augerScale_ = 0.1;
    double augerSym_ = 0;
};

} // namespace apo
