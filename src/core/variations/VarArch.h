#pragma once

#include "../Variation.h"

namespace apo {

// Not present in Apophysis 7X's Pascal source - ported from flam3's
// variations.c (var41_arch). No parameters; stochastic (uses the owning
// XForm's per-point rng).
class VarArch final : public Variation {
public:
    static constexpr const char* kName = "arch";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;
};

} // namespace apo
