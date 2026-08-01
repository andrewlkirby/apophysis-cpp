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

// FOLLOWUP_PLAN.txt B1(b): every registered variation's Variation instance
// is now constructed lazily - a fresh XForm never touches any of them (all
// weights default to 0 except linear). getVariable() must still report each
// one's real factory-default value for every one of its own registered
// parameters, not just for the single hand-picked case core_smoke_test.cpp
// covers (auger) - a per-variation subclass with an unusual default could
// only be caught by actually enumerating all of them, same rationale as
// testEveryVariationRoundTrips above.
//
// EXCEPT for a variation flagged hasNonDeterministicConstructionDefault
// (pdj/julian/julia3D/julia3Dz/juliascope/rings2/fan2/radial_blur - each
// draws its own default from constructionRandom01() at construction, not a
// fixed constant, see Variation.h) - comparing against a second, separately
// constructed `factory.create()` instance for one of these would be
// comparing two independent random draws, not verifying anything real, so
// this test only checks the deterministic-default majority here. The
// non-deterministic ones get their own, correctly-shaped check right below
// instead: idempotent repeated reads of the SAME XForm, which is the actual
// invariant getVariable()'s materialize-and-freeze behavior promises.
void testEveryVariationReadsItsDefaultsBeforeAnyTouch() {
    auto& registry = apo::VariationRegistry::instance();
    const int totalRegistered = registry.numRegisteredVariations();

    int checkedCount = 0;
    for (int i = 0; i < totalRegistered; ++i) {
        const auto& factory = registry.registeredVariation(i);
        if (factory.hasNonDeterministicConstructionDefault) continue;
        const int nVars = factory.numVariables();
        if (nVars == 0) continue;

        apo::XForm x; // never weighted, never touched - variation i is still at its factory default
        for (int j = 0; j < nVars; ++j) {
            const std::string paramName = factory.variableNameAt(j);
            double fromXForm = 0;
            const bool gotFromXForm = x.getVariable(paramName, fromXForm);

            auto reference = factory.create();
            double fromFreshInstance = 0;
            const bool gotFromInstance = reference->getVariable(paramName, fromFreshInstance);

            if (!gotFromXForm || !gotFromInstance || !approxEqual(fromXForm, fromFreshInstance, 1e-9)) {
                check(false, ("variation '" + factory.name() + "' parameter '" + paramName +
                               "' reads its true factory default before ever being touched")
                                  .c_str());
                continue;
            }
            ++checkedCount;
        }
    }

    check(checkedCount > 0, "at least one registered variation's parameter was actually exercised by this test");
}

// The non-deterministic-default counterpart to the test above: for these
// types, getVariable() must still be idempotent on repeated reads of the
// SAME never-touched XForm (materialize-and-freeze on first read, per
// XForm.cpp's own getVariable()/ensureRegVariation() comments) - two
// separate reads must agree with each other, even though neither is
// expected to match any other XForm's own frozen value.
void testNonDeterministicDefaultVariationsFreezeOnFirstRead() {
    auto& registry = apo::VariationRegistry::instance();
    const int totalRegistered = registry.numRegisteredVariations();

    int checkedCount = 0;
    for (int i = 0; i < totalRegistered; ++i) {
        const auto& factory = registry.registeredVariation(i);
        if (!factory.hasNonDeterministicConstructionDefault) continue;
        const int nVars = factory.numVariables();
        if (nVars == 0) continue;

        apo::XForm x; // never weighted, never touched
        for (int j = 0; j < nVars; ++j) {
            const std::string paramName = factory.variableNameAt(j);
            double firstRead = 0, secondRead = 0;
            const bool gotFirst = x.getVariable(paramName, firstRead);
            const bool gotSecond = x.getVariable(paramName, secondRead);

            if (!gotFirst || !gotSecond || !approxEqual(firstRead, secondRead, 1e-9)) {
                check(false, ("variation '" + factory.name() + "' parameter '" + paramName +
                               "' (non-deterministic default) reads the same value on repeated calls")
                                  .c_str());
                continue;
            }
            ++checkedCount;
        }
    }

    check(checkedCount > 0,
          "at least one non-deterministic-default variation's parameter was actually exercised by this test");
}

} // namespace

int main() {
    testEveryVariationRoundTrips();
    testEveryVariationReadsItsDefaultsBeforeAnyTouch();
    testNonDeterministicDefaultVariationsFreezeOnFirstRead();
    return apo_test::reportAndExit();
}
