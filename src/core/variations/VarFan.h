#pragma once

#include "../Variation.h"

namespace apo {

// Not present in Apophysis 7X's Pascal source - ported from flam3's
// variations.c (var22_fan). No named parameters of its own: like flam3's
// reference, it reuses the owning XForm's own affine translation
// coefficients e/f (c[2][0]/c[2][1]) as implicit wedge-width/phase
// parameters - not to be confused with "fan2" (VarFan2), a distinct,
// already-ported variation with its own independent named parameters.
class VarFan final : public Variation {
public:
    static constexpr const char* kName = "fan";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

private:
    // Precomputed in prepare() from the affine translation coefficients e/f.
    double dx_ = 0, dy_ = 0, dx2_ = 0;
};

} // namespace apo
