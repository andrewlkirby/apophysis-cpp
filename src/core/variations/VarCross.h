#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varCross.pas (TVariationCross). No parameters (the
// Pascal file's GetNrVariables/GetVariableNameAt/GetVariable/SetVariable
// overrides are all trivial, matching Variation's base defaults exactly).
class VarCross final : public Variation {
public:
    static constexpr const char* kName = "cross";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;
};

} // namespace apo
