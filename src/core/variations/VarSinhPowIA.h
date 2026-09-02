#pragma once

#include "../Variation.h"

namespace apo {

// Companion to VarSinhPow.h (see its doc comment for the map's provenance
// and the (a)/(b) framing this pair implements). Where sinhpow evaluates
// T_c(z) = z^p + sinh(c^q) exactly once per point ("(a)"), this variation
// instead runs one full step of the paper's own "IA iteration" (Definition
// 3 / Theorem 1 of Tassaddiq et al. 2026) per point ("(b)"):
//
//   x = (alpha+beta)*z + gamma*T_c(z)
//   w = (1-lambda)*T_c(x) + lambda*T_c(z)
//   y = T_c(w)
//   z' = T_c(y)
//
// - four evaluations of T_c per point (T_c(z), T_c(x), T_c(w), T_c(y))
// instead of sinhpow's one, roughly 4x the per-point cost (see the
// conversation this was scoped from: "(a) will render faster... (b) folds
// the IA scheme's whole orbit step into a single variation call").
//
// alpha/beta/gamma/lambda are exposed as four independent free parameters
// (not constrained to alpha+beta+gamma=1 at runtime, unlike the paper's own
// Mandelbrot-set escape-time use of this iteration) - flame variation
// parameters are continuously user-dragged in the editor, and the map is
// well-defined algebraically for any real values, so enforcing the
// constraint server-side would just make the parameters fight the UI.
// Defaults for alpha/beta/gamma/lambda (0.3, 0.3, 0.4, 0.25) match one of
// the paper's own example parameter sets (Figure 2) - but c's default does
// NOT match the paper's own c=1: verified empirically (rendering an actual
// test flame) that c=1 makes four compounded T_c evaluations diverge past
// the render canvas for essentially every starting point, so a freshly
// added sinhpowia xform at the paper's own default would render as an
// empty attractor (0 points generated - the chaos game's initial
// fuse/settle phase never finds a point that stays bounded). The paper
// only ever needed c=1 to stay bounded for its own escape-time coloring
// use (a fixed pixel grid checking every c independently), not for a
// single c value to sustain a self-referential chaos-game orbit under four
// compounded evaluations - a materially different boundedness requirement.
// c=0.3+0.2i (small magnitude) was verified to render a real attractor.
class VarSinhPowIA final : public Variation {
public:
    static constexpr const char* kName = "sinhpowia";
    static constexpr bool kSupports3D = true;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 8; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    double power_ = 2.0;  // p
    double q_ = 1.0;      // q
    double cx_ = 0.3;     // Re(c)
    double cy_ = 0.2;     // Im(c)
    double alpha_ = 0.3;
    double beta_ = 0.3;
    double gamma_ = 0.4;
    double lambda_ = 0.25;
};

} // namespace apo
