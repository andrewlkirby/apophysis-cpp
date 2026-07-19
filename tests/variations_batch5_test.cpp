// Hand-computed smoke tests for batch 5: VarPostFalloff2, VarPreFalloff2,
// VarPreBwraps, VarPreCrop, VarPreDisc, VarPreSinusoidal, VarPreSpherical.
//
// The falloff2 variations and pre_bwraps/pre_crop are stochastic in the
// general case, so each test picks parameters that make the random term
// vanish (mul_x=mul_y=mul_z=mul_c=0, or a geometric no-op condition) rather
// than trying to reverse-engineer the RNG stream - this verifies the
// deterministic parts of the formula (the parts most likely to contain a
// transcription error) without needing bit-exact RNG replication.

#include <cmath>

#include "TestHelpers.h"
#include "core/Rng.h"
#include "core/VariationRegistry.h"
#include "core/XForm.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

int registeredIndex(const char* name) { return apo::VariationRegistry::instance().variationIndex(name); }

// Activates `name` (weight 1) alongside linear (weight 1, index 0) so a
// pre_/post_ variation's effect is observable in the final output point -
// pre_ variations feed linear's input, post_ variations consume linear's
// output. Returns the resulting point for a fixed input.
apo::Point3 runWithLinear(const char* name, apo::Point3 in) {
    apo::XForm x;
    x.setVariation(registeredIndex(name), 1.0);
    // vars[0] (linear) already defaults to 1 - leave it on.
    apo::MtRng rng(99);
    x.prepare(rng);
    double colorCoord = 0.2;
    x.nextPoint(in, colorCoord);
    return in;
}

void testPostFalloff2PassthroughWithZeroMultipliers() {
    const int idx = registeredIndex("post_falloff2");
    apo::XForm x;
    x.setVariation(idx, 1.0);
    double v = 0;
    v = 0; x.setVariable("post_falloff2_mul_x", v);
    v = 0; x.setVariable("post_falloff2_mul_y", v);
    v = 0; x.setVariable("post_falloff2_mul_z", v);
    v = 0; x.setVariable("post_falloff2_mul_c", v);
    // vars[0] (linear) stays on, feeding post_falloff2's *px/*py/*pz input.

    apo::MtRng rng(1);
    x.prepare(rng);
    apo::Point3 pt{0.3, -0.2, 0.5};
    double colorCoord = 0.2;
    x.nextPoint(pt, colorCoord);

    // linear (vvar=1, identity affine) writes Px=tx=0.3, Py=ty=-0.2, Pz=tz=0.5;
    // post_falloff2 (vvar=1, all mul_*=0) then overwrites with
    // vvar*(in+0) = in unchanged.
    check(approxEqual(pt.x, 0.3) && approxEqual(pt.y, -0.2) && approxEqual(pt.z, 0.5),
          "post_falloff2 with all mul_* = 0 passes through the linear-accumulated point unchanged");
}

void testPreFalloff2PassthroughWithZeroMultipliers() {
    const int idx = registeredIndex("pre_falloff2");
    apo::XForm x;
    x.setVariation(idx, 1.0);
    double v = 0;
    v = 0; x.setVariable("pre_falloff2_mul_x", v);
    v = 0; x.setVariable("pre_falloff2_mul_y", v);
    v = 0; x.setVariable("pre_falloff2_mul_z", v);
    v = 0; x.setVariable("pre_falloff2_mul_c", v);

    apo::MtRng rng(2);
    x.prepare(rng);
    apo::Point3 pt{0.4, 0.1, -0.3};
    double colorCoord = 0.2;
    x.nextPoint(pt, colorCoord);

    // pre_falloff2 overwrites tx/ty/tz with vvar*(in+0) = in (identity,
    // since mul_*=0), then linear reads that unchanged tx/ty/tz.
    check(approxEqual(pt.x, 0.4) && approxEqual(pt.y, 0.1) && approxEqual(pt.z, -0.3),
          "pre_falloff2 with all mul_* = 0 passes the point through to linear unchanged");
}

void testPreBwrapsNoOpWhenCellsizeZero() {
    const int idx = registeredIndex("pre_bwraps");
    apo::XForm x;
    x.setVariation(idx, 1.0);
    double v = 0;
    x.setVariable("pre_bwraps_cellsize", v); // 0 => the Pascal `if cellsize<>0` guard skips entirely

    apo::MtRng rng(3);
    x.prepare(rng);
    apo::Point3 pt{0.25, -0.6, 0.1};
    double colorCoord = 0.2;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.25) && approxEqual(pt.y, -0.6) && approxEqual(pt.z, 0.1),
          "pre_bwraps with cellsize=0 is a genuine no-op (not scaled by vvar), point passes through via linear");
}

void testPreCropPassthroughInsideDefaultBounds() {
    // Default bounds are [-1,1]x[-1,1]; a point well inside them never hits
    // the crop/scatter branches, so x'=vvar*x, y'=vvar*y exactly.
    apo::Point3 pt = runWithLinear("pre_crop", {0.3, -0.2, 0.7});
    check(approxEqual(pt.x, 0.3) && approxEqual(pt.y, -0.2),
          "pre_crop passes a point inside the default [-1,1] bounds through unchanged");
}

void testPreDiscMatchesHandComputation() {
    constexpr double kPi = 3.14159265358979323846;
    apo::Point3 in{0.3, 0.6, 0.4};
    apo::Point3 pt = runWithLinear("pre_disc", in);

    const double t = kPi * std::sqrt(in.x * in.x + in.y * in.y);
    const double sinr = std::sin(t), cosr = std::cos(t);
    // arctan2(FTx, FTy): Y=in.x, X=in.y (swapped, matches XForm's own Disc).
    const double r = (1.0 / kPi) * std::atan2(in.x, in.y);
    const double expectedX = sinr * r;
    const double expectedY = cosr * r;

    check(approxEqual(pt.x, expectedX) && approxEqual(pt.y, expectedY),
          "pre_disc matches the hand-computed disc formula (chained through linear)");
}

void testPreSinusoidalMatchesHandComputation() {
    apo::Point3 in{0.5, -0.7, 0.2};
    apo::Point3 pt = runWithLinear("pre_sinusoidal", in);

    check(approxEqual(pt.x, std::sin(in.x)) && approxEqual(pt.y, std::sin(in.y)),
          "pre_sinusoidal matches sin(x)/sin(y) (chained through linear)");
}

void testPreSphericalMatchesHandComputation() {
    apo::Point3 in{0.6, 0.3, 0.1};
    apo::Point3 pt = runWithLinear("pre_spherical", in);

    const double r = 1.0 / (in.x * in.x + in.y * in.y + 10e-6);
    check(approxEqual(pt.x, in.x * r) && approxEqual(pt.y, in.y * r),
          "pre_spherical matches the hand-computed inverse-radius formula (chained through linear)");
}

} // namespace

int main() {
    testPostFalloff2PassthroughWithZeroMultipliers();
    testPreFalloff2PassthroughWithZeroMultipliers();
    testPreBwrapsNoOpWhenCellsizeZero();
    testPreCropPassthroughInsideDefaultBounds();
    testPreDiscMatchesHandComputation();
    testPreSinusoidalMatchesHandComputation();
    testPreSphericalMatchesHandComputation();

    return apo_test::reportAndExit();
}
