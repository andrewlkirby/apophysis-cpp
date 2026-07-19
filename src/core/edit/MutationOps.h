#pragma once

#include <memory>
#include <vector>

#include "../Flame.h"
#include "../Rng.h"

// Pure, Qt-free flame-mutation primitives backing the Mutate dialog (Phase
// 6) - mirrors GradientOps.h's role for the Adjust dialog's gradient tab.
// Together these correspond to Mutate.pas/ControlPoint.pas's RandomCP,
// SetVariation, and InterpolateX, but deliberately simplified per the
// "good performance, not a faithful copy" project direction - see each
// function's own comment for what was cut and why.
namespace apo {

// Replaces `flame`'s xform topology with a fresh random one: a random
// count of xforms in [minXforms, maxXforms], each with a random affine
// transform and a single shared variation (see `variationIndex` below).
// Matches ControlPoint.pas's RandomCP, except:
//   - Doesn't touch cmap/hue_rotation (RandomCP randomizes a colormap too,
//     but Mutate.pas's own Interpolate()/ShowMain() unconditionally
//     overwrite it with the base flame's cmap immediately after generating
//     every mutant anyway - see Mutate.pas lines 300-302 - so porting that
//     part of RandomCP would be dead work for this port's only caller).
//   - Doesn't auto-frame the camera (RandomCP's `calc` parameter driving
//     CalcBoundbox). Mutate.pas's default (mnuResetLocation unchecked)
//     already discards each mutant's own framing in favor of the base
//     flame's fixed camera, so this port always behaves that way rather
//     than building CalcBoundbox's iterative bisection just to throw its
//     result away by default; a flame that ends up poorly framed after
//     adopting a mutant can already be reframed via the Adjust dialog.
//   - Camera/render-quality/cmap fields on `flame` are left completely
//     untouched (only xform[0..N) are rewritten) - the caller decides what
//     those should be (see blendXforms below).
//
// variationIndex >= 0 forces every generated xform to that one shared
// variation (matches RandomCP's dominant rv>=0 branch, and is what the
// Mutate dialog's Trend combo box selects); < 0 draws ONE random variation
// shared across every xform of this call (matches SetVariation(vRandom)'s
// own dominant rv>=0 branch - the rarer independently-random-per-xform
// rv<0 branch needs weighted var_distrib/mixed_var_distrib tables this
// port hasn't built, and is a low-value flavor variant, not behavior worth
// the extra porting cost here).
//
// `eligibleVariations`, when variationIndex < 0, restricts the random draw
// to this list of VariationRegistry indices instead of every registered
// variation - matches Options.pas's Variations enable/disable checklist
// (ControlPoint.pas:1263+/RndFlame.pas:238-252's "use only variations set
// in options"). nullptr (the default) or an empty list both mean "every
// variation is eligible", exactly this function's pre-existing behavior -
// so every caller that doesn't care about the checklist is unaffected.
//
// The trailing six parameters extend the "single shared variation" draw
// above into a shared *palette* of variations, all still defaulted to
// exactly reproduce the original one-variation-at-weight-1.0 behavior for
// any caller that doesn't pass them (MutateDialog's two call sites, every
// existing test):
//   - minVariationsPerXform/maxVariationsPerXform: how many distinct
//     variations get drawn (once, shared by every xform in this call - not
//     redrawn per xform, so the whole flame still reads as one coherent
//     "family" rather than each xform being unrelated) from the eligible
//     pool. Ignored when variationIndex >= 0 (a forced variation is always
//     exactly that one, matching every existing forced-variation caller's
//     expectation).
//   - minVariationWeight/maxVariationWeight: each xform independently
//     draws its own weight (uniform in this range) for each of the shared
//     palette's variations - the palette (which variations) is shared
//     across the flame, but how much each contributes to any given xform
//     is not. Also ignored when variationIndex >= 0: a forced variation is
//     always weight exactly 1.0, the same pre-blending guarantee
//     testHonorsForcedVariationIndex depends on - weight ranges are a
//     "Random" (blended) draw concept only.
//   - randomizeVariationParameters: when true, every chosen variation's own
//     named parameters (e.g. wedge_angle, pdj_a) are randomized too, once
//     per xform per chosen variation - see MutationOps.cpp's
//     randomizeVariationParams() for how the range is derived (anchored on
//     the variation's own class default, not a hand-authored per-parameter
//     table - see that function's own comment for why). No-op for local
//     variations (linear, sinusoidal, ...): none of them have named
//     parameters.
//   - parameterRandomizationStrength: scales the perturbation amplitude
//     randomizeVariationParams() computes; 1.0 is the default/baseline
//     spread, <1 stays closer to the class defaults, >1 explores further.
void randomizeXforms(Flame& flame, Rng& rng, int minXforms, int maxXforms, int variationIndex,
                      const std::vector<int>* eligibleVariations = nullptr, int minVariationsPerXform = 1,
                      int maxVariationsPerXform = 1, double minVariationWeight = 1.0, double maxVariationWeight = 1.0,
                      bool randomizeVariationParameters = false, double parameterRandomizationStrength = 1.0);

// Reassigns every xform's variation (topology/coefficients untouched) to
// one shared variation - matches ControlPoint.pas's SetVariation, used by
// the Mutate dialog's Trend combo box to re-flavor already-generated
// random xforms without rerolling their affine coefficients. Same
// variationIndex/eligibleVariations convention as randomizeXforms.
void setXformsVariation(Flame& flame, Rng& rng, int variationIndex,
                         const std::vector<int>* eligibleVariations = nullptr);

// Blends xform topology between `base` (t=0) and `target` (t=1) into a
// freshly cloned flame - camera, colormap, background, and render-quality
// fields all come from `base` unchanged (not blended). Matches
// ControlPoint.pas's InterpolateX's *net* behavior for every field Mutate
// actually uses, not its literal implementation: InterpolateX does blend
// brightness/gamma/vibrancy/cmap/center/zoom/pixels_per_unit too, but
// Mutate.pas's ShowMain/ShowMutants unconditionally overwrite every one of
// those right back to the base flame's own values immediately after
// calling it (see e.g. Mutate.pas lines 174-179, 210-212) - so actually
// porting that blending work would be entirely discarded by this port's
// only caller. The final xform is likewise always copied unchanged from
// `base`, matching the original's own net behavior after its elaborate
// slot-swapping dance (see InterpolateX's own comment: "finalxform was
// supposed to be mutate-able too, but somehow it's always getting confused
// by random-generated mutants").
std::unique_ptr<Flame> blendXforms(const Flame& base, const Flame& target, double t);

} // namespace apo
