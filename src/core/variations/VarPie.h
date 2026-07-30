#pragma once

#include "../Variation.h"

namespace apo {

// Not present in Apophysis 7X's Pascal source - ported from flam3's
// variations.c (var37_pie). Defaults (slices=6, rotation=0, thickness=0.5)
// match flam3's own reference defaults, which are non-degenerate as-is
// (unlike perspective's zeroed defaults - see VarPerspective.h). Stochastic
// (uses the owning XForm's per-point rng). flam3's own pie is 2D-only, but
// every other ported variation here - including other 2D-only-in-flam3 ones
// like "log" - still passes tz through as vvar*tz and reports
// kSupports3D=true; matched here for the same reason (each active
// variation's own weighted z contribution, not just some of them).
class VarPie final : public Variation {
public:
    static constexpr const char* kName = "pie";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 3; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;

private:
    double pieSlices_ = 6.0;
    double pieRotation_ = 0.0;
    double pieThickness_ = 0.5;
};

} // namespace apo
