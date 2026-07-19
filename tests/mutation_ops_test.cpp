// Tests for the Mutate dialog's flame-mutation math (src/core/edit/
// MutationOps.h) - kept Qt-free and unit-tested independently of the
// dialog itself, mirroring gradient_ops_test.cpp's approach for the
// Adjust dialog's Gradient tab.

#include <algorithm>
#include <memory>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/Rng.h"
#include "core/VariationRegistry.h"
#include "core/edit/MutationOps.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::unique_ptr<apo::Flame> makeBaseFlame() {
    auto flame = std::make_unique<apo::Flame>();
    flame->width = 64;
    flame->height = 64;
    flame->zoom = 1.5;
    flame->center = {0.25, -0.5};
    flame->pixelsPerUnit = 40;
    flame->brightness = 3.0;
    flame->gamma = 2.5;
    flame->vibrancy = 0.5;
    flame->background = {10, 20, 30, 255};
    for (int i = 0; i < 256; ++i) flame->cmap.entries[i] = {i, 255 - i, 128, 255};

    apo::XForm& xf0 = *flame->xform[0];
    xf0.clear();
    xf0.density = 0.5;
    xf0.color = 0.0;
    xf0.c[2] = {0.1, 0.2};
    apo::XForm& xf1 = *flame->xform[1];
    xf1.clear();
    xf1.density = 0.5;
    xf1.color = 1.0;
    xf1.c[2] = {-0.1, -0.2};

    apo::XForm& fx = *flame->finalXform;
    fx.clear();
    fx.c[2] = {9.0, 9.0}; // a distinctive value, to confirm blendXforms never touches this

    return flame;
}

void testRandomizeXformsProducesXformCountInRange() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(42);

    for (int trial = 0; trial < 50; ++trial) {
        apo::randomizeXforms(*flame, rng, /*minXforms=*/2, /*maxXforms=*/5, /*variationIndex=*/0);
        const int n = flame->numXForms();
        if (!check(n >= 2 && n <= 5, "randomizeXforms always produces an xform count within [min,max]")) break;
    }
}

void testRandomizeXformsIsDeterministicGivenSeed() {
    auto flameA = std::make_unique<apo::Flame>();
    auto flameB = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rngA(123);
    apo::Xoshiro256Rng rngB(123);

    apo::randomizeXforms(*flameA, rngA, 2, 6, -1);
    apo::randomizeXforms(*flameB, rngB, 2, 6, -1);

    check(flameA->numXForms() == flameB->numXForms(), "same seed -> same xform count");
    for (int i = 0; i < flameA->numXForms(); ++i) {
        check(approxEqual(flameA->xform[i]->c[2][0], flameB->xform[i]->c[2][0]),
              "same seed -> identical affine coefficients");
    }
}

void testRandomizeXformsForcesRequestedVariation() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(7);
    const int variationIndex = 3; // spherical, per VariationRegistry::localVarNames()

    apo::randomizeXforms(*flame, rng, 3, 3, variationIndex);
    check(flame->numXForms() == 3, "min==max forces an exact xform count");
    for (int i = 0; i < flame->numXForms(); ++i) {
        const apo::XForm& xf = *flame->xform[i];
        check(approxEqual(xf.variation(variationIndex), 1.0), "every xform gets weight 1 on the requested variation");
        for (int j = 0; j < xf.numVariations(); ++j) {
            if (j == variationIndex) continue;
            if (!check(approxEqual(xf.variation(j), 0.0), "every other variation's weight is exactly 0")) return;
        }
    }
}

void testRandomizeXformsSharesOneVariationAcrossAllXforms() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(99);
    apo::randomizeXforms(*flame, rng, 4, 4, /*variationIndex=*/-1); // "Random" trend

    int sharedVariation = -1;
    for (int i = 0; i < flame->numXForms(); ++i) {
        const apo::XForm& xf = *flame->xform[i];
        for (int j = 0; j < xf.numVariations(); ++j) {
            if (xf.variation(j) > 0.5) {
                if (sharedVariation < 0) sharedVariation = j;
                check(j == sharedVariation, "a 'Random' trend still picks ONE variation shared by every xform");
            }
        }
    }
}

void testRandomizeXformsRestrictsRandomDrawToEligibleVariations() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(11);
    const std::vector<int> eligible = {2, 4}; // which two indices doesn't matter, only that the draw stays within them

    for (int trial = 0; trial < 30; ++trial) {
        apo::randomizeXforms(*flame, rng, 2, 2, /*variationIndex=*/-1, &eligible);
        const apo::XForm& xf = *flame->xform[0];
        bool matchesEligible = false;
        for (int idx : eligible) {
            if (approxEqual(xf.variation(idx), 1.0)) matchesEligible = true;
        }
        if (!check(matchesEligible, "a restricted random draw only ever lands on one of the eligible indices")) break;
    }
}

void testRandomizeXformsForcedVariationIgnoresEligibleList() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(12);
    const std::vector<int> eligible = {2, 4}; // deliberately excludes 7

    apo::randomizeXforms(*flame, rng, 2, 2, /*variationIndex=*/7, &eligible);
    check(approxEqual(flame->xform[0]->variation(7), 1.0),
          "an explicitly forced variation is used even if it isn't in eligibleVariations");
}

void testRandomizeXformsNullOrEmptyEligibleListMeansEveryVariationIsEligible() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rngA(13);
    apo::Xoshiro256Rng rngB(13);
    const std::vector<int> emptyList;

    apo::randomizeXforms(*flame, rngA, 2, 2, -1, nullptr);
    const double afterNullptr = flame->xform[0]->variation(0);
    apo::randomizeXforms(*flame, rngB, 2, 2, -1, &emptyList);
    const double afterEmpty = flame->xform[0]->variation(0);
    check(approxEqual(afterNullptr, afterEmpty),
          "a null eligibleVariations pointer and an empty (but non-null) list behave identically");
}

void testRandomizeXformsBlendPaletteIsSharedAcrossXforms() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(21);
    const std::vector<int> eligible = {2, 4, 6, 8, 10}; // five distinct candidates

    apo::randomizeXforms(*flame, rng, /*minXforms=*/4, /*maxXforms=*/4, /*variationIndex=*/-1, &eligible,
                          /*minVariationsPerXform=*/3, /*maxVariationsPerXform=*/3);

    std::vector<int> palette0;
    for (int j : eligible) {
        if (flame->xform[0]->variation(j) > 0.0) palette0.push_back(j);
    }
    if (!check(static_cast<int>(palette0.size()) == 3, "the drawn palette has exactly the requested variation count"))
        return;

    for (int i = 1; i < flame->numXForms(); ++i) {
        for (int j : eligible) {
            const bool activeHere = flame->xform[i]->variation(j) > 0.0;
            const bool activeInPalette = std::find(palette0.begin(), palette0.end(), j) != palette0.end();
            if (!check(activeHere == activeInPalette,
                        "every xform shares the exact same variation palette (weights may still differ)"))
                return;
        }
    }
}

void testRandomizeXformsBlendCountIsCappedByPoolSize() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(22);
    const std::vector<int> eligible = {3, 9}; // only two candidates

    apo::randomizeXforms(*flame, rng, 2, 2, -1, &eligible, /*minVariationsPerXform=*/1, /*maxVariationsPerXform=*/6);

    int activeCount = 0;
    for (int j : eligible) {
        if (flame->xform[0]->variation(j) > 0.0) ++activeCount;
    }
    check(activeCount <= 2, "a requested blend count above the eligible pool size is capped, not looped forever");
}

void testRandomizeXformsBlendWeightsStayWithinRange() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(23);
    const std::vector<int> eligible = {1, 2, 3, 4, 5};

    for (int trial = 0; trial < 20; ++trial) {
        apo::randomizeXforms(*flame, rng, 3, 3, -1, &eligible, 2, 3, /*minVariationWeight=*/0.3,
                              /*maxVariationWeight=*/0.9);
        for (int i = 0; i < flame->numXForms(); ++i) {
            for (int j : eligible) {
                const double w = flame->xform[i]->variation(j);
                if (w <= 0.0) continue;
                if (!check(w >= 0.3 - 1e-9 && w <= 0.9 + 1e-9,
                            "every blended variation's weight falls within [minVariationWeight,maxVariationWeight]"))
                    return;
            }
        }
    }
}

void testRandomizeXformsDefaultBlendRangeStillForcesWeightExactlyOne() {
    // The blend parameters' defaults (min=max=1 variation, weight [1,1])
    // must reproduce the pre-blending behavior exactly, byte for byte in
    // outcome if not in RNG draw sequence - every existing caller that
    // doesn't pass them (MutateDialog, every test above this one) depends
    // on it.
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(26);
    apo::randomizeXforms(*flame, rng, 3, 3, -1);
    for (int i = 0; i < flame->numXForms(); ++i) {
        int activeCount = 0;
        for (int j = 0; j < flame->xform[i]->numVariations(); ++j) {
            if (flame->xform[i]->variation(j) > 0.0) {
                check(approxEqual(flame->xform[i]->variation(j), 1.0),
                      "the default blend range still forces weight exactly 1.0");
                ++activeCount;
            }
        }
        check(activeCount == 1, "the default blend range still picks exactly one variation per xform");
    }
}

void testRandomizeXformsForcedVariationIgnoresWeightRangeToo() {
    // Regression: a forced variation must stay at weight exactly 1.0 even
    // when a non-default weight range is configured (e.g. MainWindow's
    // AppSettings-backed "Random Batch" defaults) - caught by
    // main_window_interaction_test.cpp's
    // testNewRandomBatchWithForcedVariationAndPresetGradient after the
    // weightLo/weightHi computation was first added without special-casing
    // variationIndex >= 0, since every test up to that point only ever
    // exercised forced-variation with the all-defaults (weight [1,1]) call
    // shape, which happened to force weight 1.0 either way.
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(27);
    apo::randomizeXforms(*flame, rng, 3, 3, /*variationIndex=*/5, nullptr, 1, 1, /*minVariationWeight=*/0.3,
                          /*maxVariationWeight=*/1.0);
    for (int i = 0; i < flame->numXForms(); ++i) {
        check(approxEqual(flame->xform[i]->variation(5), 1.0),
              "a forced variation stays at weight exactly 1.0 regardless of the configured weight range");
    }
}

void testRandomizeXformsParametersStayAtDefaultWhenDisabled() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(24);
    const int augerIndex = apo::VariationRegistry::instance().variationIndex("auger");
    if (!check(augerIndex >= 0, "auger is a registered variation")) return;

    apo::randomizeXforms(*flame, rng, 2, 2, augerIndex); // randomizeVariationParameters defaults to false

    for (int i = 0; i < flame->numXForms(); ++i) {
        double freq = 0.0;
        check(flame->xform[i]->getVariable("auger_freq", freq) && approxEqual(freq, 5.0),
              "auger_freq stays at its class default when parameter randomization is disabled");
    }
}

void testRandomizeXformsRandomizesParametersIndependentlyPerXformWhenEnabled() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(25);
    const int augerIndex = apo::VariationRegistry::instance().variationIndex("auger");
    if (!check(augerIndex >= 0, "auger is a registered variation")) return;

    apo::randomizeXforms(*flame, rng, 3, 3, augerIndex, nullptr, 1, 1, 1.0, 1.0,
                          /*randomizeVariationParameters=*/true);

    double freq0 = 0.0, freq1 = 0.0;
    check(flame->xform[0]->getVariable("auger_freq", freq0) && flame->xform[1]->getVariable("auger_freq", freq1),
          "auger_freq is readable on the randomized xforms");
    check(!approxEqual(freq0, 5.0), "parameter randomization moves auger_freq away from its class default");
    check(!approxEqual(freq0, freq1), "each xform randomizes its own copy of the parameter independently");
}

void testSetXformsVariationRestrictsRandomDrawToEligibleVariations() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(14);
    apo::randomizeXforms(*flame, rng, 3, 3, 0);
    const std::vector<int> eligible = {6};

    for (int trial = 0; trial < 10; ++trial) {
        apo::setXformsVariation(*flame, rng, /*variationIndex=*/-1, &eligible);
        if (!check(approxEqual(flame->xform[0]->variation(6), 1.0),
                    "setXformsVariation's random draw also respects eligibleVariations"))
            break;
    }
}

void testSetXformsVariationKeepsCoefficientsUnchanged() {
    auto flame = std::make_unique<apo::Flame>();
    apo::Xoshiro256Rng rng(55);
    apo::randomizeXforms(*flame, rng, 3, 3, 0);
    const double c0 = flame->xform[0]->c[2][0];
    const double density0 = flame->xform[0]->density;

    apo::setXformsVariation(*flame, rng, 5); // horseshoe

    check(approxEqual(flame->xform[0]->c[2][0], c0), "setXformsVariation leaves affine coefficients untouched");
    check(approxEqual(flame->xform[0]->density, density0), "setXformsVariation leaves density untouched");
    check(approxEqual(flame->xform[0]->variation(5), 1.0), "setXformsVariation applies the requested variation");
    check(approxEqual(flame->xform[0]->variation(0), 0.0), "setXformsVariation zeroes out the previous variation");
}

void testBlendXformsAtZeroEqualsBase() {
    auto base = makeBaseFlame();
    auto target = makeBaseFlame();
    target->xform[0]->c[2] = {5.0, 5.0};
    target->xform[0]->density = 0.9;

    auto blended = apo::blendXforms(*base, *target, 0.0);
    check(approxEqual(blended->xform[0]->c[2][0], base->xform[0]->c[2][0]), "t=0 reproduces base's xform coefficients exactly");
    check(approxEqual(blended->xform[0]->density, base->xform[0]->density), "t=0 reproduces base's xform density exactly");
}

void testBlendXformsAtOneEqualsTarget() {
    auto base = makeBaseFlame();
    auto target = makeBaseFlame();
    target->xform[0]->c[2] = {5.0, 5.0};
    target->xform[0]->density = 0.9;

    auto blended = apo::blendXforms(*base, *target, 1.0);
    check(approxEqual(blended->xform[0]->c[2][0], 5.0), "t=1 reproduces target's xform coefficients exactly");
    check(approxEqual(blended->xform[0]->density, 0.9), "t=1 reproduces target's xform density exactly");
}

void testBlendXformsAtHalfIsTheArithmeticMean() {
    auto base = makeBaseFlame();
    auto target = makeBaseFlame();
    target->xform[0]->c[2] = {5.0, 5.0};

    auto blended = apo::blendXforms(*base, *target, 0.5);
    check(approxEqual(blended->xform[0]->c[2][0], (0.1 + 5.0) / 2.0), "t=0.5 is the arithmetic mean of the two coefficients");
}

void testBlendXformsLeavesCameraAndColorFieldsAsBase() {
    auto base = makeBaseFlame();
    auto target = makeBaseFlame();
    target->zoom = 99;
    target->center = {123, 456};
    target->brightness = 77;
    target->gamma = 88;
    target->vibrancy = 66;
    target->background = {1, 2, 3, 4};
    for (int i = 0; i < 256; ++i) target->cmap.entries[i] = {255, 0, 0, 255};

    auto blended = apo::blendXforms(*base, *target, 0.5);
    check(approxEqual(blended->zoom, base->zoom), "blendXforms never blends zoom - always base's");
    check(approxEqual(blended->center[0], base->center[0]) && approxEqual(blended->center[1], base->center[1]),
          "blendXforms never blends center - always base's");
    check(approxEqual(blended->brightness, base->brightness), "blendXforms never blends brightness - always base's");
    check(approxEqual(blended->gamma, base->gamma), "blendXforms never blends gamma - always base's");
    check(approxEqual(blended->vibrancy, base->vibrancy), "blendXforms never blends vibrancy - always base's");
    check(blended->background == base->background, "blendXforms never blends background - always base's");
    check(blended->cmap.entries == base->cmap.entries, "blendXforms never blends the colormap - always base's");
}

void testBlendXformsCopiesFinalXformUnchangedFromBase() {
    auto base = makeBaseFlame();
    auto target = makeBaseFlame();
    target->finalXform->c[2] = {-1.0, -1.0};

    auto blended = apo::blendXforms(*base, *target, 0.5);
    check(approxEqual(blended->finalXform->c[2][0], 9.0) && approxEqual(blended->finalXform->c[2][1], 9.0),
          "the final xform is always copied unchanged from base, never blended toward target");
}

void testBlendXformsHandlesDifferingXformCounts() {
    auto base = makeBaseFlame(); // 2 active xforms
    auto target = std::make_unique<apo::Flame>();
    for (int i = 0; i < 4; ++i) {
        apo::XForm& xf = *target->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.c[2] = {static_cast<double>(i), 0.0};
    }

    auto blended = apo::blendXforms(*base, *target, 0.5);
    // Slot 2 exists only in target (density 1.0) - base has it cleared
    // (density 0), so a 50/50 blend should land at density 0.5, not crash
    // or leave it untouched.
    check(approxEqual(blended->xform[2]->density, 0.5),
          "a slot present only in target blends from base's implicit zero density");
    check(approxEqual(blended->xform[2]->c[2][0], 1.0), "that slot's coefficients blend from base's implicit zero, too");
}

} // namespace

int main() {
    testRandomizeXformsProducesXformCountInRange();
    testRandomizeXformsIsDeterministicGivenSeed();
    testRandomizeXformsForcesRequestedVariation();
    testRandomizeXformsSharesOneVariationAcrossAllXforms();
    testRandomizeXformsRestrictsRandomDrawToEligibleVariations();
    testRandomizeXformsForcedVariationIgnoresEligibleList();
    testRandomizeXformsNullOrEmptyEligibleListMeansEveryVariationIsEligible();
    testRandomizeXformsBlendPaletteIsSharedAcrossXforms();
    testRandomizeXformsBlendCountIsCappedByPoolSize();
    testRandomizeXformsBlendWeightsStayWithinRange();
    testRandomizeXformsDefaultBlendRangeStillForcesWeightExactlyOne();
    testRandomizeXformsForcedVariationIgnoresWeightRangeToo();
    testRandomizeXformsParametersStayAtDefaultWhenDisabled();
    testRandomizeXformsRandomizesParametersIndependentlyPerXformWhenEnabled();
    testSetXformsVariationRestrictsRandomDrawToEligibleVariations();
    testSetXformsVariationKeepsCoefficientsUnchanged();
    testBlendXformsAtZeroEqualsBase();
    testBlendXformsAtOneEqualsTarget();
    testBlendXformsAtHalfIsTheArithmeticMean();
    testBlendXformsLeavesCameraAndColorFieldsAsBase();
    testBlendXformsCopiesFinalXformUnchangedFromBase();
    testBlendXformsHandlesDifferingXformCounts();

    return apo_test::reportAndExit();
}
