#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varLazysusan.pas (TVariationLazysusan).
class VarLazysusan final : public Variation {
public:
    static constexpr const char* kName = "lazysusan";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 5; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double spin_ = 3.14159265358979323846;
    double space_ = 0;
    double twist_ = 0;
    double x_ = 0;
    double y_ = 0;
};

} // namespace apo
