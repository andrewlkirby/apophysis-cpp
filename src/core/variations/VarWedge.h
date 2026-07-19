#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varWedge.pas (TVariationWedge).
class VarWedge final : public Variation {
public:
    static constexpr const char* kName = "wedge";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    VarWedge();

    void prepare() override;
    void calc() override;

    int numVariables() const override { return 4; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double angle_;
    double hole_ = 0;
    int count_ = 2;
    double swirl_ = 0;

    double compFac_ = 0; // precomputed in prepare()
};

} // namespace apo
