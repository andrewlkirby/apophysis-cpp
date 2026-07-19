// Hand-computed smoke tests for batch 4 of Phase 2 variation ports: mobius,
// ngon, polar2, post_bwraps, post_crop, post_curl, post_curl3D.

#include <cmath>

#include "TestHelpers.h"
#include "core/Rng.h"
#include "core/VariationRegistry.h"
#include "core/variations/VarMobius.h"
#include "core/variations/VarNGon.h"
#include "core/variations/VarPolar2.h"
#include "core/variations/VarPostBwraps.h"
#include "core/variations/VarPostCrop.h"
#include "core/variations/VarPostCurl.h"
#include "core/variations/VarPostCurl3D.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

constexpr double kPi = 3.14159265358979323846;

void testAllBatch4VariationsRegistered() {
    auto& registry = apo::VariationRegistry::instance();
    const int localVars = apo::VariationRegistry::kNumLocalVars;
    check(registry.variationIndex("mobius") >= localVars, "mobius is registered");
    check(registry.variationIndex("ngon") >= localVars, "ngon is registered");
    check(registry.variationIndex("polar2") >= localVars, "polar2 is registered");
    check(registry.variationIndex("post_bwraps") >= localVars, "post_bwraps is registered");
    check(registry.variationIndex("post_crop") >= localVars, "post_crop is registered");
    check(registry.variationIndex("post_curl") >= localVars, "post_curl is registered");
    check(registry.variationIndex("post_curl3D") >= localVars, "post_curl3D is registered");
}

void testMobiusDefaultParamsIsIdentity() {
    // Re_A=1,Im_A=0,Re_B=0,Im_B=0,Re_C=0,Im_C=0,Re_D=1,Im_D=0 collapses the
    // Mobius transform to u/v = (tx+i*ty)/1, i.e. an identity map.
    apo::VarMobius v;
    double px = 0, py = 0, pz = 0, tx = 0.7, ty = -0.4, tz = 0.5, color = 0;
    v.tx = &tx; v.ty = &ty; v.tz = &tz;
    v.px = &px; v.py = &py; v.pz = &pz;
    v.color = &color;
    v.vvar = 1.0;
    v.calc();

    check(approxEqual(px, 0.7) && approxEqual(py, -0.4) && approxEqual(pz, 0.5),
          "mobius with default identity coefficients is an identity map");
}

void testNGonOnPositiveXAxis() {
    // Default ngon_sides=4, ngon_power=2, ngon_circle=1, ngon_corners=1.
    // At (1,0): rFactor=(1)^-1=1, theta=0, phi=0, amp=(1*(1/cos(0)-1)+1)=1.
    apo::VarNGon v;
    double px = 0, py = 0, pz = 0, tx = 1.0, ty = 0.0, tz = 0.5, color = 0;
    v.tx = &tx; v.ty = &ty; v.tz = &tz;
    v.px = &px; v.py = &py; v.pz = &pz;
    v.color = &color;
    v.vvar = 1.0;
    v.prepare();
    v.calc();

    check(approxEqual(px, 1.0) && approxEqual(py, 0.0) && approxEqual(pz, 0.5),
          "ngon at (1,0) with default params matches the hand-computed amp=1 case");
}

void testPolar2MatchesHandComputation() {
    apo::VarPolar2 v;
    double px = 0, py = 0, pz = 0, tx = 1.0, ty = 1.0, tz = 0.5, color = 0;
    v.tx = &tx; v.ty = &ty; v.tz = &tz;
    v.px = &px; v.py = &py; v.pz = &pz;
    v.color = &color;
    v.vvar = 1.0;
    v.prepare();
    v.calc();

    const double expectedPx = (1.0 / kPi) * std::atan2(1.0, 1.0); // ArcTan2(tx,ty), swapped order
    const double expectedPy = (0.5 / kPi) * std::log(2.0);

    check(approxEqual(px, expectedPx) && approxEqual(py, expectedPy) && approxEqual(pz, 0.5),
          "polar2 output matches a hand-computed formula, including the swapped atan2(tx,ty) argument order");
}

void testPostBwrapsOutsideRadiusLeavesOutputUntouched() {
    // Default cellsize=1, space=0, gain=1 -> radius=0.5, r2=0.25. Point
    // (0.9, 0) lands in cell centered at (0.5, 0.5) with local offset
    // (0.4, -0.5), Lx^2+Ly^2=0.41 > r2 -> outside the wrap radius.
    apo::VarPostBwraps v;
    double px = 0.9, py = 0.0, pz = 0.3, tx = 0, ty = 0, tz = 0, color = 0;
    v.tx = &tx; v.ty = &ty; v.tz = &tz;
    v.px = &px; v.py = &py; v.pz = &pz;
    v.color = &color;
    v.vvar = 1.0;
    v.prepare();
    v.calc();

    check(approxEqual(px, 0.9) && approxEqual(py, 0.0) && approxEqual(pz, 0.3),
          "post_bwraps leaves the accumulator untouched when the point falls outside the wrap cell radius");
}

void testPostCropInsideBoundsPassesThrough() {
    apo::VarPostCrop v;
    double px = 0.5, py = -0.3, pz = 0.77, tx = 0, ty = 0, tz = 0, color = 0;
    v.tx = &tx; v.ty = &ty; v.tz = &tz;
    v.px = &px; v.py = &py; v.pz = &pz;
    v.color = &color;
    v.vvar = 1.0;
    apo::MtRng rng(1);
    v.rng = &rng;
    v.prepare();
    v.calc();

    check(approxEqual(px, 0.5) && approxEqual(py, -0.3) && approxEqual(pz, 0.77),
          "post_crop passes an in-bounds point through unchanged (vvar=1) and never touches pz");
}

void testPostCurlZeroParamsIsIdentity() {
    apo::VarPostCurl v;
    double px = 0.4, py = -0.6, pz = 0.11, tx = 0, ty = 0, tz = 0, color = 0;
    v.tx = &tx; v.ty = &ty; v.tz = &tz;
    v.px = &px; v.py = &py; v.pz = &pz;
    v.color = &color;
    v.vvar = 1.0;
    v.prepare();
    v.calc();

    check(approxEqual(px, 0.4) && approxEqual(py, -0.6) && approxEqual(pz, 0.11),
          "post_curl with default zero c1/c2 is an identity map and never touches pz");
}

void testPostCurlPrepareIsIdempotent() {
    // The original (TVariationPostCurl.Prepare) had a bug: it mutated the
    // stored c1/c2 parameters in place on every prepare() call, so c1
    // silently compounded/shrank every time prepare() ran (e.g. every
    // interactive-preview re-render). Fixed in the port by deriving into
    // separate cached locals - verify the stored parameter and calc()'s
    // output are now stable across repeated prepare() calls.
    apo::VarPostCurl v;
    double c1 = 2.0;
    v.setVariable("post_curl_c1", c1);
    v.vvar = 0.5;

    double readBack = 0;
    v.prepare();
    v.getVariable("post_curl_c1", readBack);
    check(approxEqual(readBack, 2.0), "post_curl_c1 is unchanged by a single prepare() call");

    double px1 = 0.4, py1 = -0.6, tx = 0.4, ty = -0.6, tz = 0, color = 0;
    v.tx = &tx; v.ty = &ty; v.tz = &tz;
    v.px = &px1; v.py = &py1;
    v.color = &color;
    v.calc();

    v.prepare(); // second prepare() call - must not change the derived scaling
    v.getVariable("post_curl_c1", readBack);
    check(approxEqual(readBack, 2.0), "post_curl_c1 is still unchanged after a second prepare() call");

    double px2 = 0.4, py2 = -0.6;
    v.px = &px2; v.py = &py2;
    v.calc();

    check(approxEqual(px1, px2) && approxEqual(py1, py2),
          "post_curl's calc() output is identical across repeated prepare() calls (no compounding)");
}

void testPostCurl3DZeroParamsIsIdentity() {
    apo::VarPostCurl3D v;
    double px = 0.2, py = 0.3, pz = -0.1, tx = 0, ty = 0, tz = 0, color = 0;
    v.tx = &tx; v.ty = &ty; v.tz = &tz;
    v.px = &px; v.py = &py; v.pz = &pz;
    v.color = &color;
    v.vvar = 1.0;
    v.prepare();
    v.calc();

    check(approxEqual(px, 0.2) && approxEqual(py, 0.3) && approxEqual(pz, -0.1),
          "post_curl3D with default zero cx/cy/cz is an identity map (assigns all of px/py/pz, "
          "unlike post_curl/post_crop which leave pz untouched)");
}

} // namespace

int main() {
    testAllBatch4VariationsRegistered();
    testMobiusDefaultParamsIsIdentity();
    testNGonOnPositiveXAxis();
    testPolar2MatchesHandComputation();
    testPostBwrapsOutsideRadiusLeavesOutputUntouched();
    testPostCropInsideBoundsPassesThrough();
    testPostCurlZeroParamsIsIdentity();
    testPostCurlPrepareIsIdempotent();
    testPostCurl3DZeroParamsIsIdentity();

    return apo_test::reportAndExit();
}
