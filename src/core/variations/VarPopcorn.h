#pragma once

#include "../Variation.h"

namespace apo {

// Not present in Apophysis 7X's Pascal source - ported from flam3's
// variations.c (var17_popcorn). No named parameters of its own: like
// flam3's reference, it reuses the owning XForm's own affine translation
// coefficients (e/f) as implicit jitter-amount parameters.
class VarPopcorn final : public Variation {
public:
    static constexpr const char* kName = "popcorn";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;
};

} // namespace apo
