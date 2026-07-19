#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varLoonie.pas (TVariationLoonie). No public
// parameters (GetNrVariables returns 0 and every Get/Set/ResetVariable is a
// no-op in the original), so this doesn't override any of the named-
// parameter accessors - the Variation base class's defaults already match.
class VarLoonie final : public Variation {
public:
    static constexpr const char* kName = "loonie";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

private:
    double sqrVar_ = 0;
};

} // namespace apo
