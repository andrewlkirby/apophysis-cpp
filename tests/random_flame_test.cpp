// Tests for generateRandomFlame (src/core/edit/RandomFlame.h) - the
// primitive backing MainWindow's "New Random Batch..." action. The pieces
// it composes (randomizeXforms, randomGradient, autoFrameFlame) are each
// already covered independently in mutation_ops_test.cpp, gradient_ops_test.cpp,
// and auto_frame_test.cpp; this file verifies the composition itself.

#include <vector>

#include "TestHelpers.h"
#include "core/BuiltinGradients.h"
#include "core/VariationRegistry.h"
#include "core/edit/RandomFlame.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

void testProducesAWellFormedFlame() {
    auto flame = apo::generateRandomFlame(/*seed=*/1, 200, 150);

    check(flame->width == 200 && flame->height == 150, "the flame has the requested dimensions");
    check(flame->numXForms() >= apo::kDefaultMinRandomXforms && flame->numXForms() <= apo::kDefaultMaxRandomXforms,
          "the xform count falls within the default random range");

    bool hasNonBlackEntry = false;
    for (const auto& entry : flame->cmap.entries) {
        if (entry[0] != 0 || entry[1] != 0 || entry[2] != 0) {
            hasNonBlackEntry = true;
            break;
        }
    }
    check(hasNonBlackEntry, "the flame gets a real (non-all-black) random gradient");
}

void testIsDeterministicGivenSeed() {
    // Runs several seeds, not just one: which topology variation gets
    // picked is itself seed-dependent, and camera framing is only
    // deterministic when that variation doesn't have a construction-time-
    // randomized parameter of its own (e.g. julia3D's power - see
    // RandomFlame.h's doc comment on why) - trying a spread of seeds makes
    // it very likely at least one hits a variation that DOES, so this
    // test actually exercises the "usually, but not always" framing case
    // rather than getting lucky and only ever landing on simple ones.
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        auto a = apo::generateRandomFlame(seed, 100, 100);
        auto b = apo::generateRandomFlame(seed, 100, 100);

        if (!check(a->numXForms() == b->numXForms(), "the same seed produces the same xform count")) continue;
        check(approxEqual(a->xform[0]->c[2][0], b->xform[0]->c[2][0]) &&
                  approxEqual(a->xform[0]->c[2][1], b->xform[0]->c[2][1]),
              "the same seed produces identical xform coefficients");
        check(a->cmap.entries == b->cmap.entries, "the same seed produces an identical gradient");
    }
}

void testDifferentSeedsProduceDifferentFlames() {
    auto a = apo::generateRandomFlame(/*seed=*/1, 100, 100);
    auto b = apo::generateRandomFlame(/*seed=*/2, 100, 100);
    check(!(a->cmap.entries == b->cmap.entries), "different seeds produce different gradients");
}

void testHonorsExplicitXformRange() {
    auto flame = apo::generateRandomFlame(/*seed=*/5, 100, 100, /*minXforms=*/3, /*maxXforms=*/3);
    check(flame->numXForms() == 3, "an explicit min==max xform range is honored exactly");
}

void testHonorsForcedVariationIndex() {
    const int variationIndex = 5; // horseshoe, per VariationRegistry::localVarNames()
    auto flame = apo::generateRandomFlame(/*seed=*/9, 100, 100, 2, 6, variationIndex);
    for (int i = 0; i < flame->numXForms(); ++i) {
        check(approxEqual(flame->xform[i]->variation(variationIndex), 1.0),
              "every xform uses the forced variation when variationIndex >= 0");
    }
}

void testHonorsEligibleVariationsList() {
    const std::vector<int> eligible = {5}; // horseshoe only
    for (std::uint64_t seed = 1; seed <= 10; ++seed) {
        auto flame = apo::generateRandomFlame(seed, 100, 100, 2, 6, /*variationIndex=*/-1, &eligible);
        if (!check(approxEqual(flame->xform[0]->variation(5), 1.0),
                    "a restricted eligibleVariations list is honored even when variationIndex is -1 (Random)"))
            break;
    }
}

void testHonorsVariationBlendingRange() {
    const std::vector<int> eligible = {5, 6, 7, 8}; // horseshoe, polar, disc, spiral
    auto flame = apo::generateRandomFlame(/*seed=*/50, 100, 100, 2, 2, /*variationIndex=*/-1, &eligible,
                                           apo::RandomGradientSource::Calculated, nullptr,
                                           /*minVariationsPerXform=*/2, /*maxVariationsPerXform=*/2,
                                           /*minVariationWeight=*/0.2, /*maxVariationWeight=*/0.2);
    int activeCount = 0;
    for (int idx : eligible) {
        const double w = flame->xform[0]->variation(idx);
        if (w > 0.0) {
            check(approxEqual(w, 0.2), "each blended variation's weight matches the requested (degenerate) range");
            ++activeCount;
        }
    }
    check(activeCount == 2, "generateRandomFlame honors minVariationsPerXform/maxVariationsPerXform end-to-end");
}

void testHonorsRandomizeVariationParametersFlag() {
    const int augerIndex = apo::VariationRegistry::instance().variationIndex("auger");
    auto flame = apo::generateRandomFlame(/*seed=*/51, 100, 100, 2, 2, augerIndex, nullptr,
                                           apo::RandomGradientSource::Calculated, nullptr, 1, 1, 1.0, 1.0,
                                           /*randomizeVariationParameters=*/true);
    double freq = 0.0;
    check(flame->xform[0]->getVariable("auger_freq", freq) && !approxEqual(freq, 5.0),
          "generateRandomFlame honors randomizeVariationParameters end-to-end");
}

void testRandomPresetGradientSourceUsesABuiltinGradient() {
    auto flame = apo::generateRandomFlame(/*seed=*/3, 100, 100, 2, 6, -1, nullptr,
                                           apo::RandomGradientSource::RandomPreset);
    bool matchesABuiltin = false;
    for (int i = 0; i < apo::builtinGradientCount() && !matchesABuiltin; ++i) {
        if (flame->cmap.entries == apo::builtinGradient(i).entries) matchesABuiltin = true;
    }
    check(matchesABuiltin, "RandomPreset gradient source picks an exact copy of one of the built-in gradients");
}

void testCurrentGradientSourceUsesTheSuppliedColorMap() {
    apo::ColorMap supplied;
    for (int i = 0; i < 256; ++i) supplied.entries[static_cast<size_t>(i)] = {i, 0, 255 - i, 255};

    auto flame = apo::generateRandomFlame(/*seed=*/4, 100, 100, 2, 6, -1, nullptr,
                                           apo::RandomGradientSource::CurrentGradient, &supplied);
    check(flame->cmap.entries == supplied.entries,
          "CurrentGradient gradient source copies the caller-supplied ColorMap exactly");
}

void testCurrentGradientSourceFallsBackWhenNoneSupplied() {
    auto flame = apo::generateRandomFlame(/*seed=*/6, 100, 100, 2, 6, -1, nullptr,
                                           apo::RandomGradientSource::CurrentGradient, /*currentGradient=*/nullptr);
    bool hasNonBlackEntry = false;
    for (const auto& entry : flame->cmap.entries) {
        if (entry[0] != 0 || entry[1] != 0 || entry[2] != 0) hasNonBlackEntry = true;
    }
    check(hasNonBlackEntry,
          "CurrentGradient with no currentGradient supplied falls back to a real (calculated) gradient, not black");
}

} // namespace

int main() {
    testProducesAWellFormedFlame();
    testIsDeterministicGivenSeed();
    testDifferentSeedsProduceDifferentFlames();
    testHonorsExplicitXformRange();
    testHonorsForcedVariationIndex();
    testHonorsEligibleVariationsList();
    testHonorsVariationBlendingRange();
    testHonorsRandomizeVariationParametersFlag();
    testRandomPresetGradientSourceUsesABuiltinGradient();
    testCurrentGradientSourceUsesTheSuppliedColorMap();
    testCurrentGradientSourceFallsBackWhenNoneSupplied();

    return apo_test::reportAndExit();
}
