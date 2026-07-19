#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varBlurZoom.pas (TVariationBlurZoom).
class VarBlurZoom final : public Variation {
public:
    static constexpr const char* kName = "blur_zoom";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 3; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;

private:
    double length_ = 0;
    double x_ = 0;
    double y_ = 0;
};

} // namespace apo
