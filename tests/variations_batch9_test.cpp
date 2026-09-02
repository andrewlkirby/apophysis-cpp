// Hand-computed numeric checks for the sinhpowia variation (VarSinhPowIA.cpp)
// - sinhpow's "(b)" companion, folding one full step of the IA iteration
// from Tassaddiq et al., Fractal Fract. 2026, 10, 365 into a single
// variation call (see VarSinhPowIA.h's doc comment). The expected-value
// computation below is written independently against std::complex, not by
// transcribing VarSinhPowIA.cpp's own real/imaginary-pair arithmetic, so it
// exercises a genuinely different code path than production.

#include <cmath>
#include <complex>
#include <string>

#include "TestHelpers.h"
#include "core/Rng.h"
#include "core/VariationRegistry.h"
#include "core/XForm.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

using cd = std::complex<double>;

int registeredIndex(const std::string& name) {
    return apo::VariationRegistry::instance().variationIndex(name);
}

void testSinhPowIARegistered() {
    check(registeredIndex("sinhpowia") >= apo::VariationRegistry::kNumLocalVars, "sinhpowia is registered");
}

// Independent std::complex re-derivation of one IA step:
//   T_c(z) = z^p + sinh(c^q)
//   x = (alpha+beta)*z + gamma*T_c(z)
//   w = (1-lambda)*T_c(x) + lambda*T_c(z)
//   y = T_c(w)
//   z' = T_c(y)
void testSinhPowIADefaultsMatchHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("sinhpowia"), 1.0);

    apo::MtRng rng(1);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // Defaults: power=2, q=1, cx=0.3, cy=0.2, alpha=0.3, beta=0.3, gamma=0.4, lambda=0.25.
    const double power = 2.0, q = 1.0, alpha = 0.3, beta = 0.3, gamma = 0.4, lambda = 0.25;
    const cd c(0.3, 0.2);
    const cd K = std::sinh(std::pow(c, q));
    const cd z(0.3, 0.4);

    auto Tc = [&](cd v) { return std::pow(v, power) + K; };

    const cd tcZ = Tc(z);
    const cd xv = (alpha + beta) * z + gamma * tcZ;
    const cd tcX = Tc(xv);
    const cd w = (1.0 - lambda) * tcX + lambda * tcZ;
    const cd y = Tc(w);
    const cd zNext = Tc(y);

    check(approxEqual(pt.x, zNext.real()) && approxEqual(pt.y, zNext.imag()) && approxEqual(pt.z, 0.5),
          "sinhpowia with default parameters matches an independent std::complex re-derivation of one IA step");
}

void testSinhPowIAOriginWithZeroCDoesNotProduceNaN() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    const int idx = registeredIndex("sinhpowia");
    x.setVariation(idx, 1.0);
    double zero = 0.0;
    check(x.setVariable("sinhpowia_cx", zero) && x.setVariable("sinhpowia_cy", zero),
          "sinhpowia_cx/cy are settable to zero");

    apo::MtRng rng(2);
    x.prepare(rng);

    apo::Point3 pt{0.0, 0.0, 0.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(std::isfinite(pt.x) && std::isfinite(pt.y) && std::isfinite(pt.z),
          "sinhpowia at the origin with c=0 produces finite output, not NaN/inf");
}

void testSinhPowIAParametersAreSettable() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("sinhpowia"), 1.0);

    double power = 3.0, q = 2.0, cx = 0.5, cy = -0.25, alpha = 0.2, beta = 0.5, gamma = 0.3, lambda = 0.1;
    check(x.setVariable("sinhpowia_power", power) && x.setVariable("sinhpowia_q", q) &&
              x.setVariable("sinhpowia_cx", cx) && x.setVariable("sinhpowia_cy", cy) &&
              x.setVariable("sinhpowia_alpha", alpha) && x.setVariable("sinhpowia_beta", beta) &&
              x.setVariable("sinhpowia_gamma", gamma) && x.setVariable("sinhpowia_lambda", lambda),
          "sinhpowia's eight parameters are settable");

    double r[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    x.getVariable("sinhpowia_power", r[0]);
    x.getVariable("sinhpowia_q", r[1]);
    x.getVariable("sinhpowia_cx", r[2]);
    x.getVariable("sinhpowia_cy", r[3]);
    x.getVariable("sinhpowia_alpha", r[4]);
    x.getVariable("sinhpowia_beta", r[5]);
    x.getVariable("sinhpowia_gamma", r[6]);
    x.getVariable("sinhpowia_lambda", r[7]);
    check(approxEqual(r[0], 3.0) && approxEqual(r[1], 2.0) && approxEqual(r[2], 0.5) &&
              approxEqual(r[3], -0.25) && approxEqual(r[4], 0.2) && approxEqual(r[5], 0.5) &&
              approxEqual(r[6], 0.3) && approxEqual(r[7], 0.1),
          "sinhpowia's eight parameters read back what was set");
}

} // namespace

int main() {
    testSinhPowIARegistered();
    testSinhPowIADefaultsMatchHandComputation();
    testSinhPowIAOriginWithZeroCDoesNotProduceNaN();
    testSinhPowIAParametersAreSettable();

    return apo_test::reportAndExit();
}
