// Hand-computed numeric checks for the supermoire variation
// (VarSupermoire.cpp) - a novel (not ported from Delphi/flam3) variation
// inspired by Ning & Gedik, "Visualizing the moire of moire", Nat. Mater.
// 23, 1606-1607 (2024). The expected-value computation below re-derives the
// triangular-lattice/supermoire formula independently (a plain loop over
// three plane waves), not by transcribing VarSupermoire.cpp's own code.

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

double triLattice(double x, double y, double angle, double freq) {
    constexpr double kTwoPiOver3 = 2.0 * 3.14159265358979323846 / 3.0;
    double sum = 0;
    for (int k = 0; k < 3; ++k) {
        const double a = angle + k * kTwoPiOver3;
        sum += std::cos(freq * (x * std::cos(a) + y * std::sin(a)));
    }
    return sum;
}

double field(double x, double y, double freq1, double freq2, double angle1, double angle2, double strain,
             double mix) {
    const double l1 = triLattice(x, y, angle1, freq1);
    const double l2 = triLattice(x * (1 + strain), y * (1 - strain), angle2, freq2);
    return l1 + l2 + mix * l1 * l2;
}

void testSupermoireRegistered() {
    check(registeredIndex("supermoire") >= apo::VariationRegistry::kNumLocalVars, "supermoire is registered");
}

void testSupermoireHasSevenNamedVariables() {
    const int idx = registeredIndex("supermoire");
    check(idx >= 0, "supermoire is a known registered variation index");

    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(idx, 1.0);

    const char* expected[7] = {"supermoire_freq1",  "supermoire_freq2", "supermoire_angle1",
                                "supermoire_angle2", "supermoire_strain", "supermoire_mix",
                                "supermoire_scale"};
    double value = 0.0;
    bool allPresent = true;
    for (const char* name : expected) allPresent = x.getVariable(name, value) && allPresent;
    check(allPresent, "supermoire exposes all seven named variables");
}

void testSupermoireDefaultsMatchHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("supermoire"), 1.0);

    apo::MtRng rng(1);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    // Defaults: freq1=8, freq2=9, angle1=0, angle2=0.12, strain=0, mix=0.5, scale=0.2.
    const double freq1 = 8.0, freq2 = 9.0, angle1 = 0.0, angle2 = 0.12, strain = 0.0, mix = 0.5, scale = 0.2;
    const double tx = 0.3, ty = 0.4, tz = 0.5;

    const double expectedX = tx + scale * field(tx, ty, freq1, freq2, angle1, angle2, strain, mix);
    const double expectedY = ty + scale * field(ty, tx, freq1, freq2, angle1, angle2, strain, mix);

    check(approxEqual(pt.x, expectedX) && approxEqual(pt.y, expectedY) && approxEqual(pt.z, tz),
          "supermoire with default parameters matches an independent re-derivation of the lattice formula");
}

void testSupermoireOriginProducesFiniteOutput() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("supermoire"), 1.0);

    apo::MtRng rng(2);
    x.prepare(rng);

    apo::Point3 pt{0.0, 0.0, 0.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(std::isfinite(pt.x) && std::isfinite(pt.y) && std::isfinite(pt.z),
          "supermoire at the origin produces finite output, not NaN/inf");
}

void testSupermoireParametersAreSettable() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("supermoire"), 1.0);

    double freq1 = 5.0, freq2 = 6.0, angle1 = 0.3, angle2 = -0.2, strain = 0.15, mix = 1.0, scale = 0.5;
    check(x.setVariable("supermoire_freq1", freq1) && x.setVariable("supermoire_freq2", freq2) &&
              x.setVariable("supermoire_angle1", angle1) && x.setVariable("supermoire_angle2", angle2) &&
              x.setVariable("supermoire_strain", strain) && x.setVariable("supermoire_mix", mix) &&
              x.setVariable("supermoire_scale", scale),
          "supermoire's seven parameters are settable");

    double r[7] = {-1, -1, -1, -1, -1, -1, -1};
    x.getVariable("supermoire_freq1", r[0]);
    x.getVariable("supermoire_freq2", r[1]);
    x.getVariable("supermoire_angle1", r[2]);
    x.getVariable("supermoire_angle2", r[3]);
    x.getVariable("supermoire_strain", r[4]);
    x.getVariable("supermoire_mix", r[5]);
    x.getVariable("supermoire_scale", r[6]);
    check(approxEqual(r[0], 5.0) && approxEqual(r[1], 6.0) && approxEqual(r[2], 0.3) &&
              approxEqual(r[3], -0.2) && approxEqual(r[4], 0.15) && approxEqual(r[5], 1.0) &&
              approxEqual(r[6], 0.5),
          "supermoire's seven parameters read back what was set");
}

void testSupermoireResetRestoresDefaults() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("supermoire"), 1.0);

    double changed = 123.0;
    x.setVariable("supermoire_freq1", changed);
    x.setVariable("supermoire_angle2", changed);
    x.setVariable("supermoire_mix", changed);

    x.resetVariable("supermoire_freq1");
    x.resetVariable("supermoire_angle2");
    x.resetVariable("supermoire_mix");

    double freq1 = -1, angle2 = -1, mix = -1;
    x.getVariable("supermoire_freq1", freq1);
    x.getVariable("supermoire_angle2", angle2);
    x.getVariable("supermoire_mix", mix);
    check(approxEqual(freq1, 8.0) && approxEqual(angle2, 0.12) && approxEqual(mix, 0.5),
          "supermoire_freq1/angle2/mix reset to their documented defaults");
}

} // namespace

int main() {
    testSupermoireRegistered();
    testSupermoireHasSevenNamedVariables();
    testSupermoireDefaultsMatchHandComputation();
    testSupermoireOriginProducesFiniteOutput();
    testSupermoireParametersAreSettable();
    testSupermoireResetRestoresDefaults();

    return apo_test::reportAndExit();
}
