#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varPreSpherical.pas (TVariationPreSpherical). No parameters.
class VarPreSpherical final : public Variation {
public:
    static constexpr const char* kName = "pre_spherical";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;
};

} // namespace apo
