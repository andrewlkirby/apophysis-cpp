#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varPreDisc.pas (TVariationPreDisc). No parameters.
class VarPreDisc final : public Variation {
public:
    static constexpr const char* kName = "pre_disc";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

private:
    double vvarByPi_ = 0;
};

} // namespace apo
