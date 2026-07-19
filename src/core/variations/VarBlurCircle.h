#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varBlurCircle.pas (TVariationBlurCircle). No
// parameters (the Pascal file overrides GetNrVariables/GetVariableNameAt/
// GetVariable/SetVariable but all trivially, matching Variation's base
// defaults exactly - so none are overridden here). Stochastic: samples a
// random point per calc() call, mapped onto a square "circle" perimeter
// parametrization.
class VarBlurCircle final : public Variation {
public:
    static constexpr const char* kName = "blur_circle";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

private:
    double vvar4Pi_ = 0;
    double pi4_ = 0;
};

} // namespace apo
