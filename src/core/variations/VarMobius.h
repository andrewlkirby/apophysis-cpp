#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varMobius.pas (TVariationMobius). Prepare() is a
// no-op in the original (empty override) - not overridden here either.
class VarMobius final : public Variation {
public:
    static constexpr const char* kName = "mobius";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 8; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    // Names/casing match the Pascal fields and their exact .flame XML
    // attribute names ("Re_A" etc, case-sensitive) - not renamed to
    // lower_snake_case like most other variations' parameters.
    double reA_ = 1, imA_ = 0;
    double reB_ = 0, imB_ = 0;
    double reC_ = 0, imC_ = 0;
    double reD_ = 1, imD_ = 0;
};

} // namespace apo
