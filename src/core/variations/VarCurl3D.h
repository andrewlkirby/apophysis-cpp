#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varCurl3D.pas (TVariationCurl3D).
//
// NOT overriding selectCalcFunction(): the original's GetCalcFunction
// computes specialized fast-path branches (CalcCx/CalcCy/CalcCz/CalcLinear
// for when one or more of cx/cy/cz is zero) but then has a trailing,
// unconditional `f := CalcFunction;` right before the closing `end;` that
// discards the branch selection every single time - confirmed by reading
// the whole function, not a formatting artifact. The specialized branches
// are dead code in the shipped app; it always runs the generic path. This
// port reproduces that observed behavior by simply not having a
// specialized dispatch at all, rather than "fixing" it to take the branches
// the original's own dead code suggests were intended.
class VarCurl3D final : public Variation {
public:
    static constexpr const char* kName = "curl3D";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 3; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double cx_ = 0, cy_ = 0, cz_ = 0;

    // Precomputed in prepare().
    double cx2_ = 0, cy2_ = 0, cz2_ = 0, c2_ = 0;
    double c2x_ = 0, c2y_ = 0, c2z_ = 0;
};

} // namespace apo
