// Hand-computed smoke tests for the batch-6 variation ports: radial_blur,
// rectangles, rings2, scry, separation, splits, waves2, wedge.

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

int activateOnly(apo::XForm& x, const std::string& name, double weight) {
    x.setVariation(0, 0.0); // turn off default linear
    const int index = apo::VariationRegistry::instance().variationIndex(name);
    x.setVariation(index, weight);
    return index;
}

void testRadialBlurRegistered() {
    check(apo::VariationRegistry::instance().variationIndex("radial_blur") >= apo::VariationRegistry::kNumLocalVars,
          "radial_blur is registered");
}

void testRadialBlurZDeterministic() {
    // The stochastic terms only ever affect px/py; pz is always exactly
    // vvar*tz regardless of RNG, in every one of calc()/calcZoom()/calcSpin().
    apo::XForm x;
    activateOnly(x, "radial_blur", 1.0);

    apo::MtRng rng(99);
    x.prepare(rng);

    apo::Point3 pt{0.4, -0.7, 1.25};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.z, 1.25), "radial_blur pz is always vvar*tz regardless of the stochastic terms");
}

void testRadialBlurResetToggle() {
    apo::XForm x;
    double v = 5.0;
    x.setVariable("radial_blur_angle", v); // angle != 0 now
    check(x.resetVariable("radial_blur_angle"), "radial_blur_angle resetVariable succeeds");
    double after = -1;
    x.getVariable("radial_blur_angle", after);
    check(approxEqual(after, 0.0), "resetVariable toggles a nonzero angle to 0 (not a fixed default)");

    check(x.resetVariable("radial_blur_angle"), "radial_blur_angle resetVariable succeeds again");
    x.getVariable("radial_blur_angle", after);
    check(approxEqual(after, 1.0), "resetVariable toggles a zero angle to 1");
}

void testRectanglesDefault() {
    apo::XForm x;
    activateOnly(x, "rectangles", 1.0);

    apo::MtRng rng(1);
    x.prepare(rng);

    apo::Point3 pt{1.5, 2.5, 0.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // Defaults rectangles_x = rectangles_y = 1.
    const double expectedX = (2 * std::floor(1.5 / 1.0) + 1) * 1.0 - 1.5;
    const double expectedY = (2 * std::floor(2.5 / 1.0) + 1) * 1.0 - 2.5;
    check(approxEqual(pt.x, expectedX) && approxEqual(pt.y, expectedY),
          "rectangles with default params matches hand-computed formula");
}

void testRectanglesZeroXAvoidsDivision() {
    apo::XForm x;
    activateOnly(x, "rectangles", 1.0);
    double zero = 0.0;
    x.setVariable("rectangles_x", zero);

    apo::MtRng rng(2);
    x.prepare(rng);

    apo::Point3 pt{3.0, 2.5, 0.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // calcZeroX: px passes through unchanged (identity), avoiding the
    // floor(tx/0) division the generic calc() would hit.
    const double expectedY = (2 * std::floor(2.5 / 1.0) + 1) * 1.0 - 2.5;
    check(approxEqual(pt.x, 3.0) && approxEqual(pt.y, expectedY),
          "rectangles_x=0 takes the calcZeroX fast path (px passes through unchanged)");
}

void testRings2ExplicitVal() {
    apo::XForm x;
    activateOnly(x, "rings2", 1.0);
    double val = 1.0;
    x.setVariable("rings2_val", val);

    apo::MtRng rng(3);
    x.prepare(rng);

    apo::Point3 pt{3.0, 4.0, 0.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double dx = 1.0 * 1.0 + 1e-10;
    const double length = std::sqrt(3.0 * 3.0 + 4.0 * 4.0);
    const double r = 1.0 * (2 - dx * (std::trunc((length / dx + 1) / 2) * 2 / length + 1));
    check(approxEqual(pt.x, r * 3.0) && approxEqual(pt.y, r * 4.0),
          "rings2 with explicit val=1 matches hand-computed formula");
}

void testScryKnownPoint() {
    apo::XForm x;
    activateOnly(x, "scry", 1.0);

    apo::MtRng rng(4);
    x.prepare(rng);

    apo::Point3 pt{1.0, 0.0, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // v = 1/vvar = 1 (vvar=1). t = 1. r = 1/(sqrt(1)*(1+1)) = 0.5.
    check(approxEqual(pt.x, 0.5) && approxEqual(pt.y, 0.0) && approxEqual(pt.z, 0.5),
          "scry at (1,0) with vvar=1 matches hand-computed formula");
}

void testSeparationDefault() {
    apo::XForm x;
    activateOnly(x, "separation", 1.0);

    apo::MtRng rng(5);
    x.prepare(rng);

    apo::Point3 pt{2.0, -3.0, 0.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // Defaults: separation_x=separation_y=1, insides=0.
    const double expectedX = std::sqrt(2.0 * 2.0 + 1.0 * 1.0); // tx>0 branch
    const double expectedY = -std::sqrt(3.0 * 3.0 + 1.0 * 1.0); // ty<=0 branch
    check(approxEqual(pt.x, expectedX) && approxEqual(pt.y, expectedY),
          "separation with default params matches hand-computed formula");
}

void testSplitsExplicit() {
    apo::XForm x;
    activateOnly(x, "splits", 1.0);
    double sx = 0.5, sy = 0.5;
    x.setVariable("splits_x", sx);
    x.setVariable("splits_y", sy);

    apo::MtRng rng(6);
    x.prepare(rng);

    apo::Point3 pt{2.0, -3.0, 0.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 2.5) && approxEqual(pt.y, -3.5),
          "splits with splits_x=splits_y=0.5 matches hand-computed formula");
}

void testWaves2Default() {
    apo::XForm x;
    activateOnly(x, "waves2", 1.0);

    apo::MtRng rng(7);
    x.prepare(rng);

    apo::Point3 pt{1.0, 0.5, 2.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // Defaults: freqx=2,scalex=1,freqy=2,scaley=1,freqz=0,scalez=0.
    const double expectedX = 1.0 + 1.0 * std::sin(0.5 * 2);
    const double expectedY = 0.5 + 1.0 * std::sin(1.0 * 2);
    const double expectedZ = 2.0; // scalez=0 default, so the sin term vanishes
    check(approxEqual(pt.x, expectedX) && approxEqual(pt.y, expectedY) && approxEqual(pt.z, expectedZ),
          "waves2 with default params matches hand-computed formula");
}

void testWedgeDefaultAtOrigin() {
    apo::XForm x;
    activateOnly(x, "wedge", 1.0);

    apo::MtRng rng(8);
    x.prepare(rng);

    apo::Point3 pt{1.0, 0.0, 3.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // Defaults: angle=pi/2, hole=0, count=2, swirl=0.
    // r=1, a=atan2(0,1)+0=0, c=floor((2*0+pi)/(2*pi))=floor(0.5)=0,
    // a=0*compFac+0*angle=0 -> sin=0, cos=1 -> r'=vvar*(1+0)=1.
    check(approxEqual(pt.x, 1.0) && approxEqual(pt.y, 0.0) && approxEqual(pt.z, 3.0),
          "wedge with default params at (1,0) matches hand-computed formula");
}

} // namespace

int main() {
    testRadialBlurRegistered();
    testRadialBlurZDeterministic();
    testRadialBlurResetToggle();
    testRectanglesDefault();
    testRectanglesZeroXAvoidsDivision();
    testRings2ExplicitVal();
    testScryKnownPoint();
    testSeparationDefault();
    testSplitsExplicit();
    testWaves2Default();
    testWedgeDefaultAtOrigin();

    return apo_test::reportAndExit();
}
