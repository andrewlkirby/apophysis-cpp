// Hand-computed numeric checks for the sinhpow variation (VarSinhPow.cpp) -
// a new port, not present in Apophysis 7X or flam3, implementing the
// transcendental map T_c(z) = z^p + sinh(c^q) from Tassaddiq et al.,
// Fractal Fract. 2026, 10, 365 (see VarSinhPow.h's doc comment).

#include <cmath>
#include <string>

#include "TestHelpers.h"
#include "core/Rng.h"
#include "core/VariationRegistry.h"
#include "core/XForm.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

int registeredIndex(const std::string& name) {
    return apo::VariationRegistry::instance().variationIndex(name);
}

void testSinhPowRegistered() {
    check(registeredIndex("sinhpow") >= apo::VariationRegistry::kNumLocalVars, "sinhpow is registered");
}

// Defaults: power=2, q=1, cx=1, cy=0. c^q = (1,0), so sinh(c^q) = (sinh(1), 0)
// - a real constant. z^2 for z=(0.3,0.4) is the standard complex square
// (tx^2-ty^2, 2*tx*ty) = (-0.07, 0.24); adding the constant gives the
// expected output below.
void testSinhPowDefaultsMatchHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("sinhpow"), 1.0);

    apo::MtRng rng(1);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double kRe = std::sinh(1.0);
    const double expectedX = (0.3 * 0.3 - 0.4 * 0.4) + kRe;
    const double expectedY = (2 * 0.3 * 0.4);

    check(approxEqual(pt.x, expectedX) && approxEqual(pt.y, expectedY) && approxEqual(pt.z, 0.5),
          "sinhpow with default parameters matches a hand-computed z^2 + sinh(1) formula");
}

void testSinhPowOriginWithZeroCDoesNotProduceNaN() {
    // Edge case: z=0 (pow(0, power/2) with power>0 is exactly 0, not NaN)
    // and c=0 (atan2(0,0)=0, so c^q collapses to 0 and sinh(0)=0) - both
    // degenerate inputs the paper's own domain excludes (c != 0), but the
    // variation must not produce NaN/inf for them since flame parameters
    // are user-editable and can pass through 0 while being dragged.
    apo::XForm x;
    x.setVariation(0, 0.0);
    const int idx = registeredIndex("sinhpow");
    x.setVariation(idx, 1.0);
    double zero = 0.0;
    check(x.setVariable("sinhpow_cx", zero) && x.setVariable("sinhpow_cy", zero),
          "sinhpow_cx/cy are settable to zero");

    apo::MtRng rng(2);
    x.prepare(rng);

    apo::Point3 pt{0.0, 0.0, 0.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(std::isfinite(pt.x) && std::isfinite(pt.y) && std::isfinite(pt.z),
          "sinhpow at the origin with c=0 produces finite output, not NaN/inf");
}

void testSinhPowParametersAreSettable() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("sinhpow"), 1.0);

    double power = 3.0, q = 2.0, cx = 0.5, cy = -0.25;
    check(x.setVariable("sinhpow_power", power) && x.setVariable("sinhpow_q", q) &&
              x.setVariable("sinhpow_cx", cx) && x.setVariable("sinhpow_cy", cy),
          "sinhpow's four parameters are settable");

    double readPower = -1, readQ = -1, readCx = -1, readCy = -1;
    x.getVariable("sinhpow_power", readPower);
    x.getVariable("sinhpow_q", readQ);
    x.getVariable("sinhpow_cx", readCx);
    x.getVariable("sinhpow_cy", readCy);
    check(approxEqual(readPower, 3.0) && approxEqual(readQ, 2.0) && approxEqual(readCx, 0.5) &&
              approxEqual(readCy, -0.25),
          "sinhpow's four parameters read back what was set");
}

} // namespace

int main() {
    testSinhPowRegistered();
    testSinhPowDefaultsMatchHandComputation();
    testSinhPowOriginWithZeroCDoesNotProduceNaN();
    testSinhPowParametersAreSettable();

    return apo_test::reportAndExit();
}
