#include "RandomFlame.h"

#include "GradientOps.h"
#include "MutationOps.h"
#include "../BuiltinGradients.h"
#include "../Rng.h"
#include "../render/AutoFrame.h"

namespace apo {

namespace {
// Same splitmix64-style constant Renderer.cpp already uses to derive
// distinct per-thread seeds from one caller-given seed - reused here to
// derive three independent-enough streams (topology, gradient, framing)
// from a single `seed` parameter without needing three separate seed
// arguments at every call site.
constexpr std::uint64_t kSplitmixGamma = 0x9e3779b97f4a7c15ULL;
} // namespace

std::unique_ptr<Flame> generateRandomFlame(std::uint64_t seed, int width, int height, int minXforms, int maxXforms,
                                            int variationIndex, const std::vector<int>* eligibleVariations,
                                            RandomGradientSource gradientSource, const ColorMap* currentGradient,
                                            int minVariationsPerXform, int maxVariationsPerXform,
                                            double minVariationWeight, double maxVariationWeight,
                                            bool randomizeVariationParameters, double parameterRandomizationStrength) {
    auto flame = std::make_unique<Flame>();
    flame->width = width;
    flame->height = height;

    Xoshiro256Rng topologyRng(seed);
    randomizeXforms(*flame, topologyRng, minXforms, maxXforms, variationIndex, eligibleVariations,
                     minVariationsPerXform, maxVariationsPerXform, minVariationWeight, maxVariationWeight,
                     randomizeVariationParameters, parameterRandomizationStrength);

    // Falls back to Calculated (rather than e.g. asserting) whenever a
    // more specific source has nothing to actually draw from - no built-in
    // gradients compiled in, or no currentGradient supplied - so a caller
    // can always pass RandomPreset/CurrentGradient unconditionally without
    // having to pre-check either possibility itself.
    Xoshiro256Rng gradientRng(seed + kSplitmixGamma);
    if (gradientSource == RandomGradientSource::RandomPreset && builtinGradientCount() > 0) {
        flame->cmap = builtinGradient(gradientRng.uniformInt(builtinGradientCount()));
    } else if (gradientSource == RandomGradientSource::CurrentGradient && currentGradient) {
        flame->cmap = *currentGradient;
    } else {
        flame->cmap = randomGradient(gradientRng);
    }

    autoFrameFlame(*flame, seed + 2 * kSplitmixGamma);

    return flame;
}

} // namespace apo
