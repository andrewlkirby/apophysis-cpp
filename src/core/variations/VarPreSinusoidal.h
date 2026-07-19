#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varPreSinusoidal.pas (TVariationPreSinusoidal). No parameters.
class VarPreSinusoidal final : public Variation {
public:
    static constexpr const char* kName = "pre_sinusoidal";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;
};

} // namespace apo
