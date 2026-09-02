#pragma once

#include "../Variation.h"

namespace apo {

// New variation (no Apophysis 7X/flam3 precedent) implementing the
// transcendental map T_c(z) = z^p + sinh(c^q) studied in Tassaddiq et al.,
// "A Visual and Quantitative Study of Fractal Mandelbrot Sets Using the
// IA-Iterative Algorithm for Complex Functions", Fractal Fract. 2026, 10,
// 365 - that paper generates escape-time Mandelbrot sets by iterating T_c
// across a whole orbit under a 4-step relaxation scheme; this variation
// instead evaluates T_c exactly once per chaos-game point (the "(a)" option:
// see conversation), the natural single-evaluation flame counterpart to
// existing complex-map variations (julian, julia3Dz, mobius).
//
// c and q only ever appear together as c^q, and c/q are fixed per-xform
// parameters (not the iterated point) - so sinh(c^q) is a single constant
// complex number, independent of the point being transformed. Unlike most
// registered variations' prepare()-derived constants, this one is NOT
// precomputed and cached in prepare(): see VariationKinds.cpp's "Batch 2"
// convention (bipolar/escher/etc.) that this class of variation follows -
// the GPU device function recomputes it inline from the raw params every
// call rather than needing a host-side precompute step threaded through
// DeviceFlame's param packing, so this CPU side matches that shape by
// folding the sinh(c^q) computation into calc() too, computed fresh each
// point (cheap: one complex pow + one complex sinh, no branch/RNG needed
// since both are single-valued principal-branch evaluations).
//
// p and q are treated as continuous real exponents via the principal branch
// of complex exponentiation (z^w = exp(w*log(z)), r>=0 branch) rather than
// restricted to the paper's integer p>=2, q>=1 - the same generalization
// VarPower.cpp already makes for its own single real exponent (continuous,
// not integer-restricted), extended here to both exponents.
class VarSinhPow final : public Variation {
public:
    static constexpr const char* kName = "sinhpow";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 4; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double power_ = 2.0; // p
    double q_ = 1.0;     // q
    double cx_ = 1.0;    // Re(c) - c must be nonzero (paper's domain c != 0)
    double cy_ = 0.0;    // Im(c)
};

} // namespace apo
