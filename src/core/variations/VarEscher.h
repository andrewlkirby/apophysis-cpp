#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varEscher.pas (TVariationEscher). The Pascal
// fields are named `escher_beta, c, d` - `c`/`d` are renamed here
// (escC_/escD_) since they'd otherwise shadow the inherited affine
// coefficients Variation::c/Variation::d.
class VarEscher final : public Variation {
public:
    static constexpr const char* kName = "escher";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 1; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double escherBeta_ = 0;

    // Precomputed in prepare() from escherBeta_.
    double escC_ = 0, escD_ = 0;
};

} // namespace apo
