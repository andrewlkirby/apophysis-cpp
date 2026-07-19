#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varBlurPixelize.pas (TVariationBlurPixelize).
class VarBlurPixelize final : public Variation {
public:
    static constexpr const char* kName = "blur_pixelize";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 2; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double size_ = 0.1;
    double scale_ = 1;

    // Precomputed in prepare().
    double invSize_ = 0;
    double v_ = 0;
};

} // namespace apo
