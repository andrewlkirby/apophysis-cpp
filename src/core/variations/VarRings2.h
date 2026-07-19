#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varRings2.pas (TVariationRings2). `rings2_val`
// defaults to a random value in [0, 2), matching `FVal := Random * 2` in
// the original's Create.
class VarRings2 final : public Variation {
public:
    static constexpr const char* kName = "rings2";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    VarRings2();

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 1; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;

private:
    double val_;
    double dx_ = 0;
};

} // namespace apo
