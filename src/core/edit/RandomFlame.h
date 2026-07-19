#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "../Flame.h"

namespace apo {

// Default xform-count range for a freshly-generated random flame - now
// user-configurable from Options.pas's Random tab equivalent (see
// AppSettings.h's randomMinXforms/randomMaxXforms) - these remain the
// class defaults for any caller that doesn't override them (tests, and
// AppSettings itself before a user ever opens Options).
constexpr int kDefaultMinRandomXforms = 2;
constexpr int kDefaultMaxRandomXforms = 6;

// The subset of Options.pas's 5-way "On random flame use..." gradient-
// source choice (grpGradient/randGradient - confirmed live at
// RndFlame.pas:185, `case randGradient of`, and Main.pas:932/1073; an
// earlier pass at this port's own research had wrongly marked it dead -
// see MISSING_FEATURES_PLAN.txt's correction) that this port implements.
// "Default gradient" and "random from file" are skipped: neither has
// anything to draw from in this port (no template-gradient concept, no
// gradient-file browsing UI wired to random generation) - a real "good
// performance, not a faithful copy" cut, not an oversight.
enum class RandomGradientSource {
    RandomPreset,    // one of the ~700 built-in gradients (BuiltinGradients.h), chosen at random
    CurrentGradient, // whatever ColorMap the caller passes via `currentGradient` below
    Calculated,      // GradientOps.h's randomGradient() - this port's original, and still default, behavior
};

// Generates one complete, ready-to-render random flame: topology (see
// MutationOps.h's randomizeXforms - a single shared random variation
// across a random xform count), a random color gradient (GradientOps.h's
// randomGradient), and a camera framing that actually shows the result
// (AutoFrame.h). Matches what Main.pas's RandomBatch produces per entry -
// MutationOps.h's own randomizeXforms deliberately stops short of this
// (see its header comment): Mutate's candidates always borrow the base
// flame's existing cmap/camera, but a standalone random flame has no base
// to borrow from, so needs all three pieces composed together here.
//
// `seed` drives the topology, gradient, and framing draws (distinctly,
// internally) - the xform count, coefficients, and chosen variation, plus
// the gradient, are exactly deterministic given the same seed. The
// resulting camera framing usually is too, but isn't *guaranteed* to be:
// this constructs a brand-new Flame every call, and a handful of
// variations (e.g. julia3D) randomize one of their own parameters at
// construction time using true entropy, not a seeded Rng - a deliberate
// design from this port's own variation library (see Rng.h's
// constructionRandom01 doc comment for why: it matches the original's
// TVariationXxx.Create calling the RTL's global Random() once, and is
// explicitly not part of the seeded-render-reproducibility contract,
// since the normal render path only ever *clones* an already-constructed
// Flame - see Renderer::render()'s own determinism guarantee, which relies
// on exactly that). If the randomly-chosen topology variation happens to
// be one of those, the sampled attractor - and so its framing - can differ
// between two calls with the same seed, even though everything this
// function itself controls does not.
//
// `variationIndex`/`eligibleVariations` pass straight through to
// randomizeXforms (MutationOps.h) - see its own doc comment for their
// exact meaning (Options.pas's "Variation" menu / Variations checklist,
// respectively). `gradientSource`/`currentGradient` select which of the
// three supported gradient sources above to use; `currentGradient` is
// only read when gradientSource is CurrentGradient (a null pointer there
// falls back to Calculated, since there's nothing else to use).
//
// The trailing five parameters also pass straight through to
// randomizeXforms - see its own doc comment for exactly what each one
// does. All default to reproducing this function's original single-
// shared-variation-at-weight-1.0 behavior, so any existing caller that
// doesn't pass them is unaffected.
std::unique_ptr<Flame> generateRandomFlame(std::uint64_t seed, int width, int height,
                                            int minXforms = kDefaultMinRandomXforms,
                                            int maxXforms = kDefaultMaxRandomXforms, int variationIndex = -1,
                                            const std::vector<int>* eligibleVariations = nullptr,
                                            RandomGradientSource gradientSource = RandomGradientSource::Calculated,
                                            const ColorMap* currentGradient = nullptr,
                                            int minVariationsPerXform = 1, int maxVariationsPerXform = 1,
                                            double minVariationWeight = 1.0, double maxVariationWeight = 1.0,
                                            bool randomizeVariationParameters = false,
                                            double parameterRandomizationStrength = 1.0);

} // namespace apo
