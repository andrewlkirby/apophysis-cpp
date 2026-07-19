#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varScry.pas (TVariationScry). Exposes no
// user-settable parameters (GetNrVariables returns 0 in the original, and
// GetVariable/SetVariable/GetVariableNameAt/ResetVariable all
// unconditionally return false/'' regardless of input) - so this port
// simply doesn't override any of the Variation base class's
// parameter-access methods; the base defaults already match that behavior
// exactly.
class VarScry final : public Variation {
public:
    static constexpr const char* kName = "scry";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

private:
    double v_ = 0;
};

} // namespace apo
