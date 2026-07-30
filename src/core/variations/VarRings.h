#pragma once

#include "../Variation.h"

namespace apo {

// Not present in Apophysis 7X's Pascal source - ported from flam3's
// variations.c (var21_rings). No named parameters of its own: like flam3's
// reference, it reuses the owning XForm's own affine translation
// coefficient e (c[2][0]) as an implicit ring-spacing parameter - not to be
// confused with "rings2" (VarRings2), a distinct, already-ported variation
// with its own independent named parameter.
class VarRings final : public Variation {
public:
    static constexpr const char* kName = "rings";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

private:
    // Precomputed in prepare() from the affine translation coefficient e.
    double dx_ = 0;
};

} // namespace apo
