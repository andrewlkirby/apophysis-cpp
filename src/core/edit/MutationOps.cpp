#include "MutationOps.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>

#include "../VariationRegistry.h"
#include "../XForm.h"

namespace apo {

namespace {
int pickVariation(Rng& rng, int variationIndex, const std::vector<int>* eligibleVariations) {
    if (variationIndex >= 0) return variationIndex;
    if (eligibleVariations && !eligibleVariations->empty()) {
        return (*eligibleVariations)[static_cast<size_t>(rng.uniformInt(static_cast<int>(eligibleVariations->size())))];
    }
    return rng.uniformInt(VariationRegistry::instance().nrVar());
}

// Every VariationRegistry index eligible for a "Random" draw: `eligible`
// verbatim if non-null/non-empty, otherwise every index in the combined
// local+registered space - matches pickVariation's own fallback so a
// blended draw and a single-variation draw agree on what "every variation"
// means.
std::vector<int> eligiblePool(const std::vector<int>* eligibleVariations) {
    if (eligibleVariations && !eligibleVariations->empty()) return *eligibleVariations;
    std::vector<int> pool(static_cast<size_t>(VariationRegistry::instance().nrVar()));
    std::iota(pool.begin(), pool.end(), 0);
    return pool;
}

// Fills `out` with `count` distinct indices drawn from `pool` (partial
// Fisher-Yates over a local copy). `count` is clamped to `pool.size()`
// first, so asking for more distinct variations than the eligible pool
// actually has just returns every eligible index once, rather than looping
// forever trying to find one more distinct pick.
void pickDistinctVariations(Rng& rng, const std::vector<int>& pool, int count, std::vector<int>& out) {
    std::vector<int> candidates = pool;
    count = std::clamp(count, 1, static_cast<int>(candidates.size()));
    out.clear();
    out.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        const int j = i + rng.uniformInt(static_cast<int>(candidates.size()) - i);
        std::swap(candidates[static_cast<size_t>(i)], candidates[static_cast<size_t>(j)]);
        out.push_back(candidates[static_cast<size_t>(i)]);
    }
}

// Randomizes every named parameter of registered variation `variationIndex`
// on `xf`, anchored on that parameter's own class default rather than a
// hand-authored per-parameter range table: `resetVariable` already encodes
// each variation's own sensible starting point (e.g. auger_freq -> 5,
// log_base -> e, wedge_angle -> pi/2 - ported straight from the original's
// own TVariationXxx.ResetVariable), so perturbing symmetrically around it
// keeps the random draw in the same neighborhood the variation's own author
// considered reasonable, for any variation - present now or added later -
// without this file needing to know anything variation-specific. The
// perturbation amplitude is `strength * max(1.0, |default|)`: the 1.0 floor
// matches the affine-coefficient scale already used a few lines below
// (`2 * rng.uniform01() - 1`), so a parameter that defaults to 0 still gets
// a meaningful spread instead of staying pinned at 0. No-op for local
// variations (index < kNumLocalVars): Variation::numVariables() defaults to
// 0 for all of them (linear, sinusoidal, ... have no named parameters at
// all, matching flam3/Apophysis's own local-variation set).
void randomizeVariationParams(XForm& xf, Rng& rng, int variationIndex, double strength) {
    if (variationIndex < VariationRegistry::kNumLocalVars) return;
    const auto& registry = VariationRegistry::instance();
    const VariationFactory& factory = registry.registeredVariation(variationIndex - VariationRegistry::kNumLocalVars);
    for (int i = 0; i < factory.numVariables(); ++i) {
        const std::string name = factory.variableNameAt(i);
        xf.resetVariable(name);
        double value = 0.0;
        if (!xf.getVariable(name, value)) continue;
        const double amplitude = strength * std::max(1.0, std::fabs(value));
        double randomized = value + amplitude * (2 * rng.uniform01() - 1);
        xf.setVariable(name, randomized);
    }
}
} // namespace

void randomizeXforms(Flame& flame, Rng& rng, int minXforms, int maxXforms, int variationIndex,
                      const std::vector<int>* eligibleVariations, int minVariationsPerXform,
                      int maxVariationsPerXform, double minVariationWeight, double maxVariationWeight,
                      bool randomizeVariationParameters, double parameterRandomizationStrength) {
    for (int i = 0; i < Flame::kMaxXForms; ++i) flame.xform[i]->clear();

    const int lo = std::max(1, std::min(minXforms, maxXforms));
    const int hi = std::min(Flame::kMaxXForms, std::max(minXforms, maxXforms));
    const int n = lo + rng.uniformInt(hi - lo + 1);

    // The shared variation "palette" for this whole flame - drawn once, not
    // per xform, so the flame still reads as one coherent family (matches
    // the pre-existing single-shared-variation design this generalizes; see
    // this function's own header comment). A forced variationIndex is
    // always exactly a one-variation palette, ignoring the blend range
    // entirely - matches every existing forced-variation caller/test.
    std::vector<int> palette;
    if (variationIndex >= 0) {
        palette = {variationIndex};
    } else {
        const int blendLo = std::max(1, std::min(minVariationsPerXform, maxVariationsPerXform));
        const int blendHi = std::max(blendLo, std::max(minVariationsPerXform, maxVariationsPerXform));
        const int count = blendLo + rng.uniformInt(blendHi - blendLo + 1);
        pickDistinctVariations(rng, eligiblePool(eligibleVariations), count, palette);
    }

    // A forced variation always gets exactly weight 1.0, ignoring the
    // configured weight range entirely - matches every existing forced-
    // variation caller/test (MutateDialog's Trend combo,
    // testHonorsForcedVariationIndex) from before weighted blending
    // existed. The weight range is a "Random" (blended) draw concept only.
    const double weightLo = variationIndex >= 0 ? 1.0 : std::min(minVariationWeight, maxVariationWeight);
    const double weightHi = variationIndex >= 0 ? 1.0 : std::max(minVariationWeight, maxVariationWeight);

    for (int i = 0; i < n; ++i) {
        XForm& xf = *flame.xform[i];
        xf.density = 1.0 / n;
        xf.color = (n > 1) ? static_cast<double>(i) / (n - 1) : 0.0;

        xf.c[0][0] = 2 * rng.uniform01() - 1;
        xf.c[0][1] = 2 * rng.uniform01() - 1;
        xf.c[1][0] = 2 * rng.uniform01() - 1;
        xf.c[1][1] = 2 * rng.uniform01() - 1;
        xf.c[2][0] = 4 * rng.uniform01() - 2;
        xf.c[2][1] = 4 * rng.uniform01() - 2;

        for (int j = 0; j < xf.numVariations(); ++j) xf.setVariation(j, 0.0);
        for (int idx : palette) {
            const double weight = (weightLo >= weightHi) ? weightLo : weightLo + rng.uniform01() * (weightHi - weightLo);
            xf.setVariation(idx, weight);
            if (randomizeVariationParameters) randomizeVariationParams(xf, rng, idx, parameterRandomizationStrength);
        }
    }
}

void setXformsVariation(Flame& flame, Rng& rng, int variationIndex, const std::vector<int>* eligibleVariations) {
    const int variation = pickVariation(rng, variationIndex, eligibleVariations);
    const int n = flame.numXForms();
    for (int i = 0; i < n; ++i) {
        XForm& xf = *flame.xform[i];
        for (int j = 0; j < xf.numVariations(); ++j) xf.setVariation(j, 0.0);
        xf.setVariation(variation, 1.0);
    }
}

std::unique_ptr<Flame> blendXforms(const Flame& base, const Flame& target, double t) {
    auto out = base.clone();
    const double c0 = 1.0 - t;
    const double c1 = t;
    const int n = std::max(base.numXForms(), target.numXForms());

    for (int i = 0; i < n; ++i) {
        XForm& o = *out->xform[i];
        const XForm& a = *base.xform[i];
        const XForm& b = *target.xform[i];

        o.density = c0 * a.density + c1 * b.density;
        o.color = c0 * a.color + c1 * b.color;
        for (int j = 0; j < 3; ++j) {
            o.c[j][0] = c0 * a.c[j][0] + c1 * b.c[j][0];
            o.c[j][1] = c0 * a.c[j][1] + c1 * b.c[j][1];
        }

        for (int j = 0; j < o.numVariations(); ++j) {
            o.setVariation(j, c0 * a.variation(j) + c1 * b.variation(j));
        }
        o.interpolateVariablesFrom(a, b, c0, c1);
    }

    return out;
}

} // namespace apo
