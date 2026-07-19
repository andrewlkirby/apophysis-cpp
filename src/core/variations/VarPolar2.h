#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varPolar2.pas (TVariationPolar2). No user-facing
// parameters (GetNrVariables returns 0 in the original).
class VarPolar2 final : public Variation {
public:
    static constexpr const char* kName = "polar2";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

private:
    double p2vv_ = 0, p2vv2_ = 0;
};

} // namespace apo
