#pragma once

#include "../Variation.h"

namespace apo {

// Not present in Apophysis 7X's Pascal source - ported from flam3's
// variations.c (var15_waves). No named parameters of its own: like flam3's
// reference, it reuses the owning XForm's own affine coefficients (c/d = the
// linear y-row, e/f = the translation row) as implicit frequency/amplitude
// parameters instead of exposing separate variables - not to be confused
// with "waves2" (VarWaves2), which is a distinct, already-ported variation
// with its own independent named frequency/scale parameters.
class VarWaves final : public Variation {
public:
    static constexpr const char* kName = "waves";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

private:
    // Precomputed in prepare() from the affine translation coefficients e/f.
    double dx2_ = 0, dy2_ = 0;
};

} // namespace apo
