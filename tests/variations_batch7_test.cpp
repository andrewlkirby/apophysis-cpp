// Hand-computed numeric checks for the batch-7 variation ports: the flam3
// "core" variations that were never part of Apophysis 7X's own Pascal
// source (heart, bent, waves, popcorn, exponential, power, cosine, rings,
// fan, arch, tangent, rays, blade, secant2, twintrian, blob, perspective,
// pie - see each Var*.h for its own provenance comment). Not a replacement
// for the render-correctness harness planned for Phase 5.

#include <cmath>
#include <string>

#include "TestHelpers.h"
#include "core/Rng.h"
#include "core/VariationRegistry.h"
#include "core/XForm.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

constexpr double kPi = 3.14159265358979323846;

int registeredIndex(const std::string& name) {
    return apo::VariationRegistry::instance().variationIndex(name);
}

void testAllBatch7VariationsRegistered() {
    for (const char* name : {"heart", "bent", "waves", "popcorn", "exponential", "power", "cosine",
                              "rings", "fan", "arch", "tangent", "rays", "blade", "secant2", "twintrian",
                              "blob", "perspective", "pie"}) {
        check(registeredIndex(name) >= apo::VariationRegistry::kNumLocalVars,
              (std::string(name) + " is registered").c_str());
    }
}

void testHeartMatchesHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("heart"), 1.0);

    apo::MtRng rng(1);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double rad = std::sqrt(0.3 * 0.3 + 0.4 * 0.4);
    const double a = rad * std::atan2(0.3, 0.4);
    const double expectedX = rad * std::sin(a);
    const double expectedY = -rad * std::cos(a);

    check(approxEqual(pt.x, expectedX) && approxEqual(pt.y, expectedY) && approxEqual(pt.z, 0.5),
          "heart variation output matches a hand-computed formula for a known point");
}

void testBentNegativeQuadrant() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("bent"), 1.0);

    apo::MtRng rng(2);
    x.prepare(rng);

    apo::Point3 pt{-0.3, -0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, -0.6) && approxEqual(pt.y, -0.2) && approxEqual(pt.z, 0.5),
          "bent doubles a negative X and halves a negative Y");
}

void testPopcornIsIdentityWithDefaultAffine() {
    // popcorn's jitter amount comes from the owning XForm's own affine
    // translation coefficients (e/f), which default to 0 - so with no
    // affine translation set, popcorn is an exact passthrough.
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("popcorn"), 1.0);

    apo::MtRng rng(3);
    x.prepare(rng);

    apo::Point3 pt{0.3, -0.2, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.3) && approxEqual(pt.y, -0.2) && approxEqual(pt.z, 0.5),
          "popcorn with zero affine translation passes a point through unchanged");
}

void testWavesWithZeroXIsDeterministic() {
    // waves' frequency terms come from the owning XForm's own affine
    // coefficients; with the default identity affine (c=0, d=1, e=f=0) and
    // tx=0, both sin() arguments collapse to exactly 0 regardless of the
    // (otherwise huge) precalculated 1/(e^2+eps) term, avoiding floating-
    // point sensitivity in a hand-computed check.
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("waves"), 1.0);

    apo::MtRng rng(4);
    x.prepare(rng);

    apo::Point3 pt{0.0, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.0) && approxEqual(pt.y, 0.4) && approxEqual(pt.z, 0.5),
          "waves at tx=0 with default affine matches the degenerate-but-exact expected output");
}

void testExponentialMatchesHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("exponential"), 1.0);

    apo::MtRng rng(5);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double dx = std::exp(0.3 - 1.0);
    const double dy = kPi * 0.4;

    check(approxEqual(pt.x, dx * std::cos(dy)) && approxEqual(pt.y, dx * std::sin(dy)) &&
              approxEqual(pt.z, 0.5),
          "exponential variation output matches a hand-computed formula for a known point");
}

void testPowerMatchesHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("power"), 1.0);

    apo::MtRng rng(6);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double rad = 0.5;
    const double sina = 0.3 / rad, cosa = 0.4 / rad;
    const double r = std::pow(rad, sina);

    check(approxEqual(pt.x, r * cosa) && approxEqual(pt.y, r * sina) && approxEqual(pt.z, 0.5),
          "power variation output matches a hand-computed formula for a known point");
}

void testCosineMatchesHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("cosine"), 1.0);

    apo::MtRng rng(7);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double a = 0.3 * kPi;
    const double expectedX = std::cos(a) * std::cosh(0.4);
    const double expectedY = -std::sin(a) * std::sinh(0.4);

    check(approxEqual(pt.x, expectedX) && approxEqual(pt.y, expectedY) && approxEqual(pt.z, 0.5),
          "cosine variation output matches a hand-computed formula for a known point");
}

void testTangentMatchesHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("tangent"), 1.0);

    apo::MtRng rng(8);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, std::sin(0.3) / std::cos(0.4)) && approxEqual(pt.y, std::tan(0.4)) &&
              approxEqual(pt.z, 0.5),
          "tangent variation output matches a hand-computed formula for a known point");
}

void testSecant2MatchesHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("secant2"), 1.0);

    apo::MtRng rng(9);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double rad = std::sqrt(0.3 * 0.3 + 0.4 * 0.4);
    const double cr = std::cos(rad);
    const double icr = 1.0 / cr;
    const double expectedY = (cr < 0.0) ? icr + 1.0 : icr - 1.0;

    check(approxEqual(pt.x, 0.3) && approxEqual(pt.y, expectedY) && approxEqual(pt.z, 0.5),
          "secant2 variation output matches a hand-computed formula for a known point");
}

void testBlobDefaultsMatchHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("blob"), 1.0);

    apo::MtRng rng(10);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double rad = 0.5;
    const double angle = std::atan2(0.3, 0.4);
    const double r = rad * (0.2 + 0.8 * (0.5 + 0.5 * std::sin(angle)));
    const double sina = 0.3 / rad, cosa = 0.4 / rad;

    check(approxEqual(pt.x, sina * r) && approxEqual(pt.y, cosa * r) && approxEqual(pt.z, 0.5),
          "blob with default parameters matches a hand-computed formula for a known point");
}

void testBlobParametersAreSettable() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("blob"), 1.0);

    double low = 0.5, high = 2.0, waves = 3.0;
    check(x.setVariable("blob_low", low) && x.setVariable("blob_high", high) &&
              x.setVariable("blob_waves", waves),
          "blob's three parameters are settable");

    double readLow = -1, readHigh = -1, readWaves = -1;
    x.getVariable("blob_low", readLow);
    x.getVariable("blob_high", readHigh);
    x.getVariable("blob_waves", readWaves);
    check(approxEqual(readLow, 0.5) && approxEqual(readHigh, 2.0) && approxEqual(readWaves, 3.0),
          "blob's three parameters read back what was set");
}

void testPerspectiveMatchesHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("perspective"), 1.0);

    apo::MtRng rng(11);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // Defaults: perspective_angle = 0.5, perspective_dist = 2.0.
    const double ang = 0.5 * kPi / 2.0;
    const double vsin = std::sin(ang);
    const double vfcos = 2.0 * std::cos(ang);
    const double t = 1.0 / (2.0 - 0.4 * vsin);

    check(approxEqual(pt.x, 2.0 * 0.3 * t) && approxEqual(pt.y, vfcos * 0.4 * t) &&
              approxEqual(pt.z, 0.5),
          "perspective with default parameters matches a hand-computed formula for a known point");
}

void testRingsAndFanZComponentIsDeterministic() {
    // rings/fan derive their own frequency from the XForm's affine
    // coefficients, which makes their x/y output at the default (near-zero)
    // affine translation too floating-point-sensitive to hand-verify - but
    // the z pass-through is independent of that and still checkable.
    for (const char* name : {"rings", "fan"}) {
        apo::XForm x;
        x.setVariation(0, 0.0);
        x.setVariation(registeredIndex(name), 1.0);

        apo::MtRng rng(12);
        x.prepare(rng);

        apo::Point3 pt{0.3, 0.4, 0.42};
        double colorCoord = 0.0;
        x.nextPoint(pt, colorCoord);

        check(approxEqual(pt.z, 0.42), (std::string(name) + "'s z output is exactly vvar*tz").c_str());
    }
}

void testStochasticVariationsZComponentIsDeterministic() {
    // arch, rays, blade, twintrian, and pie all draw from the per-point
    // rng for x/y, but each still passes z through deterministically as
    // exactly vvar*tz - a partial, still-meaningful check without
    // reverse-engineering the rng stream (matches batch1's blur_circle
    // check).
    for (const char* name : {"arch", "rays", "blade", "twintrian", "pie"}) {
        apo::XForm x;
        x.setVariation(0, 0.0);
        x.setVariation(registeredIndex(name), 1.0);

        apo::MtRng rng(13);
        x.prepare(rng);

        apo::Point3 pt{0.3, 0.4, 0.42};
        double colorCoord = 0.0;
        x.nextPoint(pt, colorCoord);

        check(approxEqual(pt.z, 0.42), (std::string(name) + "'s z output is exactly vvar*tz").c_str());
    }
}

void testPieParametersAreSettable() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("pie"), 1.0);

    double slices = 8.0, rotation = 1.0, thickness = 0.25;
    check(x.setVariable("pie_slices", slices) && x.setVariable("pie_rotation", rotation) &&
              x.setVariable("pie_thickness", thickness),
          "pie's three parameters are settable");

    double readSlices = -1, readRotation = -1, readThickness = -1;
    x.getVariable("pie_slices", readSlices);
    x.getVariable("pie_rotation", readRotation);
    x.getVariable("pie_thickness", readThickness);
    check(approxEqual(readSlices, 8.0) && approxEqual(readRotation, 1.0) &&
              approxEqual(readThickness, 0.25),
          "pie's three parameters read back what was set");
}

} // namespace

int main() {
    testAllBatch7VariationsRegistered();
    testHeartMatchesHandComputation();
    testBentNegativeQuadrant();
    testPopcornIsIdentityWithDefaultAffine();
    testWavesWithZeroXIsDeterministic();
    testExponentialMatchesHandComputation();
    testPowerMatchesHandComputation();
    testCosineMatchesHandComputation();
    testTangentMatchesHandComputation();
    testSecant2MatchesHandComputation();
    testBlobDefaultsMatchHandComputation();
    testBlobParametersAreSettable();
    testPerspectiveMatchesHandComputation();
    testRingsAndFanZComponentIsDeterministic();
    testStochasticVariationsZComponentIsDeterministic();
    testPieParametersAreSettable();

    return apo_test::reportAndExit();
}
