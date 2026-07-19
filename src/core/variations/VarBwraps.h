#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varBwraps.pas (TVariationBwraps).
class VarBwraps final : public Variation {
public:
    static constexpr const char* kName = "bwraps";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 5; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double cellsize_ = 1;
    double space_ = 0;
    double gain_ = 1;
    double innerTwist_ = 0;
    double outerTwist_ = 0;

    // Precomputed in prepare().
    double g2_ = 0, r2_ = 0, rfactor_ = 0;
};

} // namespace apo
