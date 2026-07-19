#pragma once

#include "../Variation.h"

namespace apo {

// Ported from Variations/varFalloff2.pas (TVariationFalloff2). Overrides
// selectCalcFunction() to dispatch on blurtype_ (0=linear/default,
// 1=radial, 2=gaussian) - a genuine, live dispatch re-evaluated every
// prepare(). Also mutates the shared `color` value (DirectColor-style),
// not just *px/*py/*pz.
class VarFalloff2 final : public Variation {
public:
    static constexpr const char* kName = "falloff2";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void prepare() override;
    void calc() override;
    CalcFn selectCalcFunction() override;

    int numVariables() const override { return 11; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    void calcRadial();
    void calcGaussian();

    // Shared falloff-distance computation used by all three calc variants -
    // matches the (duplicated, in the original) `d := sqrt(...); if invert
    // ...; if d<0 ...; d := (d-mindist)*rmax; if d<0 ...;` block that
    // appears identically at the top of CalcFunction/CalcFunctionRadial/
    // CalcFunctionGaussian.
    double falloffDistance(double inX, double inY, double inZ) const;

    double rmax_ = 0; // precomputed in prepare(): 0.04 * scatter_
    double x0_ = 0, y0_ = 0, z0_ = 0;
    double scatter_ = 1, mindist_ = 0.5;
    int invert_ = 0;
    int blurtype_ = 0;
    double mulX_ = 1, mulY_ = 1, mulZ_ = 0, mulC_ = 0;
};

} // namespace apo
