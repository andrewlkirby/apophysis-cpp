// Hand-computed smoke tests for the batch-3 variation ports: hemisphere,
// julia3D, julia3Dz, juliascope, lazysusan, log, loonie.

#include <cmath>

#include "TestHelpers.h"
#include "core/Rng.h"
#include "core/VariationRegistry.h"
#include "core/XForm.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

int variationIndex(const char* name) { return apo::VariationRegistry::instance().variationIndex(name); }

void testHemisphere() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("hemisphere"), 1.0);

    apo::MtRng rng(1);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double t = 1.0 / std::sqrt(0.3 * 0.3 + 0.4 * 0.4 + 1);
    // Pz gets the raw scale factor t added, not vvar*Tz or t*Tz - verified
    // against the source, not assumed symmetric with Px/Py.
    check(approxEqual(pt.x, 0.3 * t) && approxEqual(pt.y, 0.4 * t) && approxEqual(pt.z, t),
          "hemisphere matches hand-computed formula, including Pz's raw-t (not vvar*Tz) quirk");
}

void testJulia3DjfPower1FastPath() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("julia3D"), 1.0);
    double power = 1.0;
    x.setVariable("julia3D_power", power);

    apo::MtRng rng(2);
    x.prepare(rng);

    apo::Point3 pt{0.6, -0.4, 0.25};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.6) && approxEqual(pt.y, -0.4) && approxEqual(pt.z, 0.25),
          "julia3D with power=1 takes the CalcPower1 passthrough fast path");
}

void testJulia3DzPower1FastPath() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("julia3Dz"), 1.0);
    double power = 1.0;
    x.setVariable("julia3Dz_power", power);

    apo::MtRng rng(3);
    x.prepare(rng);

    apo::Point3 pt{0.2, 0.7, -0.3};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.2) && approxEqual(pt.y, 0.7) && approxEqual(pt.z, -0.3),
          "julia3Dz with power=1 takes the CalcPower1 passthrough fast path");
}

void testJuliaScopePower1FastPath() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("juliascope"), 1.0);
    double power = 1.0;
    x.setVariable("juliascope_power", power);
    // juliascope_dist defaults to 1.0 already, which is required for the
    // fast-path gate (`if c = 1 then ...`) to engage at all.

    apo::MtRng rng(4);
    x.prepare(rng);

    apo::Point3 pt{-0.5, 0.5, 0.1};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, -0.5) && approxEqual(pt.y, 0.5) && approxEqual(pt.z, 0.1),
          "juliascope with power=1, dist=1 takes the CalcPower1 passthrough fast path");
}

void testLazysusanOutsideBranch() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    const int idx = variationIndex("lazysusan");
    x.setVariation(idx, 0.01); // small weight so r >= vvar, forcing the "outside" branch
    // Defaults: spin=pi (irrelevant to this branch), space=0, twist=0 (irrelevant), x=0, y=0.

    apo::MtRng rng(5);
    x.prepare(rng);

    apo::Point3 pt{1.0, 1.0, 2.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // With space=0, x=0, y=0: r' = 1 + 0/(r+eps) = 1, so Px += vvar*x,
    // Py += vvar*y, Pz += vvar*Tz - a plain scale by the weight.
    check(approxEqual(pt.x, 0.01 * 1.0) && approxEqual(pt.y, 0.01 * 1.0) && approxEqual(pt.z, 0.01 * 2.0),
          "lazysusan's outside-radius branch matches hand-computed formula with default params");
}

void testLog() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("log"), 1.0);
    // log_base defaults to e, so denom = 0.5 / ln(e) = 0.5.

    apo::MtRng rng(6);
    x.prepare(rng);

    apo::Point3 pt{1.0, 1.0, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double denom = 0.5 / std::log(2.71828182845905);
    const double expectedX = std::log(1.0 * 1.0 + 1.0 * 1.0) * denom;
    const double expectedY = std::atan2(1.0, 1.0);
    check(approxEqual(pt.x, expectedX) && approxEqual(pt.y, expectedY) && approxEqual(pt.z, 0.5),
          "log matches hand-computed formula with default base=e");
}

void testLoonieInsideBranch() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("loonie"), 2.0); // sqrvar = 4

    apo::MtRng rng(7);
    x.prepare(rng);

    apo::Point3 pt{1.0, 0.0, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // r2 = 1 < sqrvar(4) and != 0, so the "inside" branch applies:
    // r = vvar * sqrt(sqrvar/r2 - 1) = 2 * sqrt(4 - 1) = 2*sqrt(3).
    const double r = 2.0 * std::sqrt(4.0 / 1.0 - 1.0);
    check(approxEqual(pt.x, r * 1.0) && approxEqual(pt.y, 0.0) && approxEqual(pt.z, 2.0 * 0.5),
          "loonie's inside-radius branch matches hand-computed formula");
}

void testLoonieOutsideBranch() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(variationIndex("loonie"), 0.5); // sqrvar = 0.25

    apo::MtRng rng(8);
    x.prepare(rng);

    apo::Point3 pt{1.0, 0.0, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // r2 = 1 >= sqrvar(0.25), so the "outside" branch applies: a plain
    // scale by vvar.
    check(approxEqual(pt.x, 0.5 * 1.0) && approxEqual(pt.y, 0.0) && approxEqual(pt.z, 0.5 * 0.5),
          "loonie's outside-radius branch matches hand-computed formula");
}

} // namespace

int main() {
    testHemisphere();
    testJulia3DjfPower1FastPath();
    testJulia3DzPower1FastPath();
    testJuliaScopePower1FastPath();
    testLazysusanOutsideBranch();
    testLog();
    testLoonieInsideBranch();
    testLoonieOutsideBranch();

    return apo_test::reportAndExit();
}
