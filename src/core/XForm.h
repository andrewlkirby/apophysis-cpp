#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Point3.h"
#include "Rng.h"
#include "Variation.h"
#include "VariationRegistry.h"

namespace apo {

// Mirrors Delphi's TXForm (Flame/XForm.pas): one affine transform plus its
// blended variations. Owns NRLOCVAR (29) built-in "local" variations
// in-line (ported below from XForm.pas's non-asm branches - the original
// gates faster x86 asm versions behind `_ASM_`/32-bit, which this port does
// not carry forward; Phase 3 is where a portable SIMD replacement belongs,
// not here) plus one Variation instance per type registered in
// VariationRegistry (Phase 2 populates that registry; empty for now, so
// every XForm currently only has its 29 local variations available).
//
// Max transform count (Delphi's NXFORMS - default 100, T500 variant 500,
// Light variant 50) is a compile-time constant there; kept as a runtime
// constant here since nothing in this port needs the Light/T500 build
// variants.
class XForm {
public:
    static constexpr int kMaxXForms = 100; // NXFORMS (default, non-T500/Light build)
    static constexpr int kNumLocalVars = VariationRegistry::kNumLocalVars; // NRLOCVAR

    XForm();
    ~XForm();

    // c[i][0..1]: affine coefficients. c[0]=(a,d) c[1]=(b,e) c[2]=(c,f) in
    // the usual flame-file a,b,c,d,e,f order - kept as the original's
    // [row][col] layout (c[0][0]=a, c[0][1]=d, c[1][0]=b, c[1][1]=e,
    // c[2][0]=c, c[2][1]=f) rather than renaming, so ParameterIO (Phase 4)
    // can read/write the same six numbers in the same order as the .flame
    // XML `coefs="a b c d e f"` attribute without a mental remapping.
    std::array<std::array<double, 2>, 3> c{{{1, 0}, {0, 1}, {0, 0}}};
    std::array<std::array<double, 2>, 3> p{{{1, 0}, {0, 1}, {0, 0}}}; // post-transform coefs

    double density = 0;
    double color = 0;
    double color2 = 0;
    double symmetry = 0;
    bool postXswap = false;
    std::string transformName;
    bool autoZscale = false;
    double transOpacity = 1;
    double pluginColor = 1;
    int orientationType = 0;

    std::array<double, kMaxXForms + 1> modWeights; // xaos weights, default all 1

    void clear();

    // Rebuilds the per-point calc-function dispatch list from the current
    // `vars` weights and `p` post-transform coefs. Must be called after any
    // weight/coefficient change and before nextPoint(). `rng` is retained
    // for the lifetime of subsequent nextPoint() calls (until the next
    // prepare()) - see Rng.h for why this is passed explicitly rather than
    // touched globally.
    void prepare(Rng& rng);

    // Runs one point through this transform in place, matching
    // TXForm.NextPoint. `colorCoord` is the flame's running color
    // coordinate (Delphi's CPpoint.c).
    void nextPoint(Point3& pt, double& colorCoord);

    // Precomputed by prepare() (FOLLOWUP_PLAN.txt B3): true when
    // transOpacity == 1.0, the overwhelming common case for real flames -
    // Renderer.cpp's per-point opacity roll (`rng.uniform01() >=
    // transOpacity`) can only ever pass when this is true (any draw in
    // [0,1) is < 1.0), so the caller skips drawing from the RNG at all
    // rather than drawing a value whose outcome was already certain. Exact
    // equality, not a tolerance check - transOpacity is a plain settable
    // field (XForm::assign() copies it verbatim from the source flame, no
    // interpolation/rounding happens to it before render time), so a flame
    // saved with opacity="1" reads back as exactly 1.0, not something
    // merely close to it.
    bool opacityAlwaysPasses() const { return opacityAlwaysPasses_; }

    // vars[i]: interpolated weight for variation i, 0..kNumLocalVars-1 for
    // the local variations, kNumLocalVars.. for registered ones (see
    // VariationRegistry). vars[0] (linear) defaults to 1, matching Clear().
    int numVariations() const { return static_cast<int>(vars_.size()); }
    double variation(int index) const { return vars_.at(index); }
    void setVariation(int index, double value) { vars_.at(index) = value; }

    // Named-parameter access, delegating to whichever registered Variation
    // instance owns that name (matches TXForm.GetVariable/SetVariable's
    // linear scan across FRegVariations).
    bool getVariable(const std::string& name, double& value) const;
    bool setVariable(const std::string& name, double& value);
    bool resetVariable(const std::string& name);

    // Copies every field of `other` into *this, including per-variable
    // values for every registered variation (matches TXForm.Assign).
    void assign(const XForm& other);

    // Blends registered-variation parameters from two sources by weight
    // (matches TXForm.InterpolateVariablesFrom) - used by Flame's animation
    // interpolation path (Phase 3+).
    void interpolateVariablesFrom(const XForm& x1, const XForm& x2, double c0, double c1);

    // Affine-coefficient helpers (editor transform-triangle operations),
    // matching TXForm.Rotate/Translate/Multiply/Scale.
    void rotate(double degrees);
    void translate(double x, double y);
    // Left-multiplies the affine coefs by [[m00,m01],[m10,m11]] (matches
    // TXForm.Multiply(a,b,c,d) - renamed here so the params don't shadow
    // the `c` affine-coefficient member).
    void multiply(double m00, double m01, double m10, double m11);
    void scale(double s);

private:
    using Matrix3 = std::array<std::array<double, 3>, 3>;
    static Matrix3 identity();
    static Matrix3 mul33(const Matrix3& m1, const Matrix3& m2);

    void addRegVariations();
    void buildFunctionList();

    // Local variation procedures - one per kLocalVarNames entry, ported
    // 1:1 from XForm.pas's non-asm branches. All read Tx_/Ty_/Tz_ and
    // accumulate into Px_/Py_/Pz_.
    void linear3D();
    void flatten();
    void sinusoidal();
    void spherical();
    void swirl();
    void horseshoe();
    void polar();
    void disc();
    void spiral();
    void hyperbolic();
    void square(); // variable name "diamond" - see VariationRegistry::localVarNames()
    void eyefish();
    void bubble();
    void cylinder();
    void noise();
    void blur();
    void gaussian();
    void zBlur();
    void blur3D();
    void preBlur();
    void preZScale();
    void preZTranslate();
    void preRotateX();
    void preRotateY();
    void zScale();
    void zTranslate();
    void zCone();
    void postRotateX();
    void postRotateY();

    void precalcAngle();
    void precalcSinCos();
    void precalcAll();
    void doPostTransform();

    // Per-point scratch state (Delphi's FTx/FTy/FTz/FPx/FPy/FPz).
    double tx_ = 0, ty_ = 0, tz_ = 0;
    double px_ = 0, py_ = 0, pz_ = 0;
    double vc_ = 0; // color passed to registered-variation Variation::color

    // Cached affine coefs (Prepare() copies c[][]/p[][] here once per
    // prepare(), matching the original's asm-friendly flat locals).
    double c00_ = 1, c01_ = 0, c10_ = 0, c11_ = 1, c20_ = 0, c21_ = 0;
    double p00_ = 1, p01_ = 0, p10_ = 0, p11_ = 1, p20_ = 0, p21_ = 0;
    double colorC1_ = 1, colorC2_ = 0;

    double angle_ = 0, sinA_ = 0, cosA_ = 0, length_ = 0;
    double polarVpi_ = 0, discVpi_ = 0;
    std::array<double, 4> gaussRnd_{};
    int gaussN_ = 0;
    double rxSin_ = 0, rxCos_ = 0, rySin_ = 0, ryCos_ = 0;
    double pxSin_ = 0, pxCos_ = 0, pySin_ = 0, pyCos_ = 0;

    bool hasPostTransform_ = false;
    bool opacityAlwaysPasses_ = true; // see the public accessor's own comment

    std::vector<double> vars_; // kNumLocalVars + registered-variation count
    std::vector<std::unique_ptr<Variation>> regVariations_;

    // Built once (matches Delphi's FFunctionList, built in BuildFunctionlist
    // at construction time): every available calc function, local + registered,
    // indexed 0..kNumLocalVars-1 then kNumLocalVars... CalcFn, not
    // std::function<void()> - see Variation.h's own comment (FOLLOWUP_PLAN.txt
    // B2b).
    std::vector<CalcFn> functionList_;
    // Rebuilt every prepare() (matches FCalcFunctionList): the ordered subset
    // of functionList_ actually active for the current variation weights.
    std::vector<CalcFn> calcFunctionList_;

    Rng* rng_ = nullptr;
};

} // namespace apo
