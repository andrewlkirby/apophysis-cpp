// Hand-computed smoke tests for the batch-2 variation ports: curl,
// elliptic, epispiral, escher, falloff2, fan2, foci.

#include <cmath>

#include "TestHelpers.h"
#include "core/Rng.h"
#include "core/VariationRegistry.h"
#include "core/XForm.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

int variationIndex(const char* name) { return apo::VariationRegistry::instance().variationIndex(name); }

void checkRegistered(const char* name) {
    check(variationIndex(name) >= apo::VariationRegistry::kNumLocalVars, name);
}

void testAllBatch2VariationsRegistered() {
    checkRegistered("curl");
    checkRegistered("elliptic");
    checkRegistered("epispiral");
    checkRegistered("escher");
    checkRegistered("falloff2");
    checkRegistered("fan2");
    checkRegistered("foci");
}

// curl defaults c1=c2=0, which should dispatch to the CalcZeroC2C1 fast
// path - an exact linear passthrough scaled by vvar.
void testCurlDefaultIsLinearPassthrough() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("curl"), 1.0);

    apo::MtRng rng(1);
    x.prepare(rng);

    apo::Point3 pt{0.4, -0.3, 0.2};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.4) && approxEqual(pt.y, -0.3) && approxEqual(pt.z, 0.2),
          "curl with c1=c2=0 (default) is an exact linear passthrough");
}

void testEllipticZPassthrough() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("elliptic"), 1.0);

    apo::MtRng rng(2);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.75};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.z, 0.75), "elliptic's z output is exactly vvar*tz");
}

// epispiral's calc() never writes *pz at all (confirmed by reading the
// whole procedure) - the only z contribution any variation on this XForm
// could produce is therefore absent, so z should stay at its Prepare-time
// reset value of 0 regardless of input z.
void testEpispiralNeverTouchesZ() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("epispiral"), 1.0);

    apo::MtRng rng(3);
    x.prepare(rng);

    apo::Point3 pt{0.2, 0.1, 5.0}; // deliberately large/nonzero z
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.z, 0.0), "epispiral never contributes to z (confirmed quirk, not a test bug)");
}

void testEscherZPassthrough() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("escher"), 1.0);

    apo::MtRng rng(4);
    x.prepare(rng);

    apo::Point3 pt{0.2, -0.5, -1.25};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.z, -1.25), "escher's z output is exactly vvar*tz");
}

// falloff2 defaults mul_z=0, so the z term's random multiplier is always
// zeroed out regardless of the RNG draw - z should be an exact passthrough
// even though x/y/color all consume randomness.
void testFalloff2ZIsDeterministicWithDefaultMulZ() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("falloff2"), 1.0);

    apo::MtRng rng(5);
    x.prepare(rng);

    apo::Point3 pt{0.1, 0.1, 0.6};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.z, 0.6), "falloff2 with default mul_z=0 leaves z an exact passthrough despite RNG use");
}

void testFan2ZPassthrough() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("fan2"), 1.0);

    apo::MtRng rng(6);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.2, 2.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.z, 2.5), "fan2's z output is exactly vvar*tz");
}

// foci at the origin is a clean edge case: expx=expnx=0.5, cosy=1, so
// tmp=0 exactly, hitting the `if (tmp = 0) then tmp := 1e-6` guard - and
// (expx-expnx) and siny are both exactly 0, so x/y stay untouched
// regardless of the 1e6 scale factor.
void testFociAtOrigin() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("foci"), 1.0);

    apo::MtRng rng(8);
    x.prepare(rng);

    apo::Point3 pt{0.0, 0.0, 3.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.0) && approxEqual(pt.y, 0.0) && approxEqual(pt.z, 3.0),
          "foci at the origin hits the tmp==0 guard and leaves x/y at 0");
}

} // namespace

int main() {
    testAllBatch2VariationsRegistered();
    testCurlDefaultIsLinearPassthrough();
    testEllipticZPassthrough();
    testEpispiralNeverTouchesZ();
    testEscherZPassthrough();
    testFalloff2ZIsDeterministicWithDefaultMulZ();
    testFan2ZPassthrough();
    testFociAtOrigin();

    return apo_test::reportAndExit();
}
