#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varElliptic.pas (TVariationElliptic). No custom
// parameters (GetNrVariables returns 0 in the original).
class VarElliptic final : public Variation {
public:
    static constexpr const char* kName = "elliptic";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

private:
    double v_ = 0; // precomputed in prepare(): vvar / (pi/2)
};

} // namespace apo
