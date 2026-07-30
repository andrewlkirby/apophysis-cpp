#pragma once

#include "../Variation.h"

namespace apo {

// Not present in Apophysis 7X's Pascal source - ported from flam3's
// variations.c (var23_blob). Defaults (low=0.2, high=1.0, waves=1.0) are
// flam3's own reference defaults for a *used* blob (its xform-init path
// zeroes every variation's parameters uniformly before any variation is
// actually added, which isn't a meaningful per-variation default) -
// 0.2/1.0/1.0 is the commonly-cited starting point from Apophysis's own
// variation dialog and avoids blob_low=0 periodically collapsing the
// radius to exactly zero.
class VarBlob final : public Variation {
public:
    static constexpr const char* kName = "blob";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 3; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;

private:
    double blobLow_ = 0.2;
    double blobHigh_ = 1.0;
    double blobWaves_ = 1.0;
};

} // namespace apo
