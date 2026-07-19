#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varHemisphere.pas (TVariationHemisphere). No
// parameters, no constructor overrides (Create is empty in the original).
class VarHemisphere final : public Variation {
public:
    static constexpr const char* kName = "hemisphere";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;
};

} // namespace apo
