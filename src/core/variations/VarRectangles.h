#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varRectangles.pas (TVariationRectangles).
// Overrides selectCalcFunction(): when rectangles_x or rectangles_y is
// zero, the generic calc() would divide by it (`floor(FTx^/FRectanglesX)`)
// - the specialized branches exist to avoid that division, not just to go
// faster, so this dispatch is functionally required (unlike VarCurl3D's
// dead-code version, which never actually takes its specialized branches).
class VarRectangles final : public Variation {
public:
    static constexpr const char* kName = "rectangles";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;
    CalcFn selectCalcFunction() override;

    int numVariables() const override { return 2; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;

private:
    void calcZeroX();
    void calcZeroY();
    void calcZeroXY();

    double rectanglesX_ = 1.0;
    double rectanglesY_ = 1.0;
};

} // namespace apo
