// Smoke tests for the statically-linked Plugin/*.c wrappers (see
// src/core/plugins/). Exercises the two prototype plugins end-to-end
// through the same XForm::prepare()/nextPoint() path the native variation
// tests use, to confirm the C-compiled, symbol-prefixed plugin build
// actually produces correct variation output, not just that it links.

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

void testPluginsAreRegistered() {
    for (const char* name : {"bcircle", "barycentroid"}) {
        check(registeredIndex(name) >= apo::VariationRegistry::kNumLocalVars,
              (std::string(name) + " plugin is registered").c_str());
    }
}

void testBcircleInsideUnitCircleIsScaledPassthrough() {
    // bcircle_scale defaults to 1.0, bcircle_borderwidth to 0.0. For a point
    // with r = sqrt(x^2+y^2) <= 1, bcircle.c's PluginVarCalc does a plain
    // FPx += VVAR*x; FPy += VVAR*y (see thirdparty/bcircle.c) - no color/z
    // handling at all (z stays whatever XForm's linear-off passthrough left
    // it as), so this checks the r<=1 branch is an exact passthrough.
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("bcircle"), 1.0);

    apo::MtRng rng(1);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.3) && approxEqual(pt.y, 0.4),
          "bcircle with r<=1 and default scale=1 is an exact passthrough");
}

void testBcircleVariablesRoundTrip() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("bcircle"), 1.0);

    double scale = 2.5;
    check(x.setVariable("bcircle_scale", scale), "bcircle_scale is settable");

    double readBack = 0;
    x.getVariable("bcircle_scale", readBack);
    check(approxEqual(readBack, 2.5), "bcircle_scale setVariable/getVariable round-trip");

    check(x.resetVariable("bcircle_scale"), "bcircle_scale resetVariable reports success");
    x.getVariable("bcircle_scale", readBack);
    check(approxEqual(readBack, 1.0), "bcircle_scale resets to its declared default (1.0)");
}

void testBarycentroidProducesFiniteOutputAndPassesZ() {
    // Not a hand-derived formula check (the barycentric-coordinate math
    // itself isn't the point here) - this exercises the plugin adapter
    // end-to-end (PluginVarPrepare/PluginVarCalc through the hoisted
    // static sgn() helper - see thirdparty/barycentroid.c) and confirms
    // the hoisted-nested-function rewrite didn't break anything: finite,
    // non-NaN output, and FPz += VVAR*FTz (an exact passthrough since pz
    // starts at 0).
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("barycentroid"), 1.0);

    apo::MtRng rng(1);
    x.prepare(rng);

    apo::Point3 pt{0.3, -0.4, 0.75};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(std::isfinite(pt.x) && std::isfinite(pt.y) && std::isfinite(pt.z), "barycentroid output is finite");
    check(approxEqual(pt.z, 0.75), "barycentroid passes FTz through to FPz unchanged (vvar=1)");
}

void testBarycentroidVariablesRoundTrip() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("barycentroid"), 1.0);

    double a = 3.0;
    check(x.setVariable("barycentroid_a", a), "barycentroid_a is settable");

    double readBack = 0;
    x.getVariable("barycentroid_a", readBack);
    check(approxEqual(readBack, 3.0), "barycentroid_a setVariable/getVariable round-trip");
}

} // namespace

int main() {
    testPluginsAreRegistered();
    testBcircleInsideUnitCircleIsScaledPassthrough();
    testBcircleVariablesRoundTrip();
    testBarycentroidProducesFiniteOutputAndPassesZ();
    testBarycentroidVariablesRoundTrip();
    return apo_test::reportAndExit();
}
