#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varNGon.pas (TVariationNGon).
class VarNGon final : public Variation {
public:
    static constexpr const char* kName = "ngon";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 4; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    int ngonSides_ = 4;
    double ngonPower_ = 2;
    double ngonCircle_ = 1;
    double ngonCorners_ = 1;

    // Precomputed in prepare().
    double cpower_ = 0, csides_ = 0, csidesinv_ = 0;
};

} // namespace apo
