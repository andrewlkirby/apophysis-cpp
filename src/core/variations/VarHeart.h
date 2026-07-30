#pragma once

#include "../Variation.h"

namespace apo {

// Not present in Apophysis 7X's Pascal source (verified: no varHeart.pas in
// the upstream repo) - ported instead from flam3's variations.c
// (var7_heart), the reference implementation this whole variation set
// otherwise descends from. No parameters.
class VarHeart final : public Variation {
public:
    static constexpr const char* kName = "heart";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;
};

} // namespace apo
