#pragma once

#include "../Variation.h"

namespace apo {

// Not present in Apophysis 7X's Pascal source - ported from flam3's
// variations.c (var45_blade). No parameters; stochastic (uses the owning
// XForm's per-point rng). Note both output components are driven by tx
// alone (not tx/ty independently) - that's the reference's actual formula,
// not a transcription slip, and is what produces the characteristic
// blade/ribbon streaks.
class VarBlade final : public Variation {
public:
    static constexpr const char* kName = "blade";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;
};

} // namespace apo
