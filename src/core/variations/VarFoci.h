#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varFoci.pas (TVariationFoci). No custom
// parameters (GetNrVariables returns 0 in the original).
class VarFoci final : public Variation {
public:
    static constexpr const char* kName = "foci";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;
};

} // namespace apo
