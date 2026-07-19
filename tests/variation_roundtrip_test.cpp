// Phase 5 correctness harness: stresses FlameIO's variation-name/parameter
// attribute lookup (src/core/io/FlameIO.cpp) against *every* registered
// variation - all 29 local + all native/plugin ones from Phase 1/2 (94 at
// the time this was written) - not just the handful flame_io_test.cpp
// hand-picks. This is exactly the kind of thing a real-world .flame file
// could trip over that a narrow test wouldn't catch: a variation name
// containing characters XML handles unexpectedly, a parameter name that
// collides with one of FlameIO's own fixed xform attributes (weight,
// color, coefs, ...), or a registration gap where a variation exists in
// the registry but FlameIO's name lookup can't find it.

#include <cstdio>
#include <string>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/VariationRegistry.h"
#include "core/io/FlameIO.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

void testEveryVariationRoundTrips() {
    auto& registry = apo::VariationRegistry::instance();
    const int total = registry.nrVar();

    int okCount = 0;
    for (int i = 0; i < total; ++i) {
        const std::string name = registry.varName(i);

        auto flame = std::make_unique<apo::Flame>();
        flame->width = 10;
        flame->height = 10;
        apo::XForm& xf = *flame->xform[0];
        xf.clear();
        xf.density = 1.0;
        for (int v = 0; v < xf.numVariations(); ++v) xf.setVariation(v, 0.0);
        xf.setVariation(i, 0.777);

        // Also exercise one parameter, for variations that have any (every
        // registered - i.e. non-local - variation; the 29 local ones are
        // single-weight, no sub-parameters).
        std::string paramName;
        double paramValue = 0;
        bool hasParam = false;
        if (i >= apo::VariationRegistry::kNumLocalVars) {
            const auto& factory = registry.registeredVariation(i - apo::VariationRegistry::kNumLocalVars);
            if (factory.numVariables() > 0) {
                paramName = factory.variableNameAt(0);
                paramValue = 0.4242;
                xf.setVariable(paramName, paramValue);
                // setVariable can normalize the value (e.g. round to a
                // nonzero integer for some parameters) - read back what
                // actually got stored so the round-trip check compares
                // against the real post-normalization value, not the raw
                // input.
                xf.getVariable(paramName, paramValue);
                hasParam = true;
            }
        }

        const std::string path = "variation_roundtrip_" + std::to_string(i) + ".flame";
        if (!apo::saveFlameFile(path, {flame.get()})) {
            check(false, ("saveFlameFile succeeds for variation '" + name + "'").c_str());
            continue;
        }

        auto loaded = apo::loadFlameFile(path);
        std::remove(path.c_str());

        if (loaded.size() != 1 || loaded[0]->numXForms() < 1) {
            check(false, ("variation '" + name + "' round-trips through a loadable flame").c_str());
            continue;
        }

        const apo::XForm& lxf = *loaded[0]->xform[0];
        const bool weightOk = approxEqual(lxf.variation(i), 0.777, 1e-4);
        const bool paramOk = !hasParam || [&] {
            double v = 0;
            return lxf.getVariable(paramName, v) && approxEqual(v, paramValue, 1e-4);
        }();

        if (weightOk && paramOk) {
            ++okCount;
        } else {
            check(false, ("variation '" + name + "' weight" + (hasParam ? "+parameter" : "") +
                           " round-trips correctly")
                              .c_str());
        }
    }

    check(okCount == total,
          ("all registered variations round-trip through FlameIO (" + std::to_string(okCount) + "/" +
           std::to_string(total) + ")")
              .c_str());
}

} // namespace

int main() {
    testEveryVariationRoundTrips();
    return apo_test::reportAndExit();
}
