#pragma once

#include "../Variation.h"

namespace apo {

// Not present in Apophysis 7X's Pascal source - ported from flam3's
// variations.c (var19_power). No parameters.
class VarPower final : public Variation {
public:
    static constexpr const char* kName = "power";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;
};

} // namespace apo
