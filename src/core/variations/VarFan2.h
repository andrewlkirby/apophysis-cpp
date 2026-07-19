#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varFan2.pas (TVariationFan2). Parameters default
// to random values in [-1, 1), matching `2*Random-1` in the original's
// Create - see Rng.h's constructionRandom01().
class VarFan2 final : public Variation {
public:
    static constexpr const char* kName = "fan2";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    VarFan2();

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 2; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;

private:
    double fanX_, fanY_;

    // Precomputed in prepare() from fanX_/fanY_.
    double dy_ = 0, dx_ = 0, dx2_ = 0;
};

} // namespace apo
