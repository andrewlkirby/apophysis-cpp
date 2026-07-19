#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varpdj.pas (TVariationPDJ). Coefficients default
// to random values in [-3, 3), matching `6*Random-3` in the original's
// Create - see Rng.h's constructionRandom01() for why this uses a separate,
// one-time construction RNG rather than the per-render-thread one.
class VarPDJ final : public Variation {
public:
    static constexpr const char* kName = "pdj";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    VarPDJ();

    void calc() override;

    int numVariables() const override { return 4; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;

private:
    double a_, b_, c_, d_;
};

} // namespace apo
