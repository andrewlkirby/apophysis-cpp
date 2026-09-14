#pragma once

#include <cstdint>

#include "../Flame.h"

namespace apo {

// Minimum number of the (fixed, 10000-point) sample that must come back
// finite/non-diverging for the attractor to be worth framing at all - below
// this, the flame is numerically unstable and diverges (NaN/Inf) on most
// iterations. Exposed as a named default (rather than a literal buried in
// the .cpp) so AppSettings/OptionsDialog can offer it as a configurable
// "blank/degenerate detection" knob for random batch discarding (see
// AppSettings.h's randomMinFramingSamples). 500 (5% of the 10000-point
// sample) rather than the smaller value this used to default to: this only
// guards against near-total numerical divergence, so raising it costs
// nothing (the sample budget is fixed either way) and rejects more clearly
// broken flames before they'd otherwise slip through on a lucky handful of
// finite points.
constexpr int kDefaultMinValidSamples = 500;

// Sets flame.center/pixelsPerUnit so its attractor is actually visible
// within its own width/height - matches ControlPoint.pas's CalcBoundbox in
// purpose. Needed anywhere a flame's camera can't just be inherited from
// somewhere else: Mutate's mutants always keep the base flame's existing
// camera (see core/edit/MutationOps.h's header comment on why that let
// CalcBoundbox be skipped there), but a freshly-generated random flame
// (core/edit/RandomFlame.h) has no such base to borrow from - without
// this, most random flames would render blank or badly cropped, since the
// attractor could be anywhere relative to Flame's default center=(0,0).
//
// Not a port of CalcBoundbox's actual algorithm (an iterative bisection
// converging toward a ~5%-outlier bounding box over 10 rounds) - this
// computes the same kind of trimmed bounding box directly via percentiles
// over one Renderer::samplePoints() call, which is simpler and more
// precise than iteratively guessing toward the same target. `flame.zoom`
// is left untouched either way, matching the original.
//
// Returns false - leaving center/pixelsPerUnit untouched - for any of three
// distinct "not really frameable" cases, all treated as degenerate on
// purpose so a caller doing blank-discarding (random batch generation)
// catches every one of them, not just outright numerical divergence:
//   - fewer than `minValidSamples` of the sampled points came back finite
//     (the attractor diverges on most iterations);
//   - the trimmed sample cloud has collapsed to (near) a single point
//     (deltaX or deltaY <= 0.001 world units) - CalcBoundbox's original
//     near-zero-area case used to fall back to a default pixelsPerUnit=10
//     framing here, but a single-point attractor is visually
//     indistinguishable from blank at any zoom, so this port treats it as
//     degenerate instead of pretending it framed something;
//   - the trimmed sample cloud is implausibly large (deltaX or deltaY >
//     1000 world units) - CalcBoundbox's original fallback here forced
//     center=(0,0), which usually isn't anywhere near the actual
//     attractor and so usually renders blank; same reasoning, now
//     degenerate instead of a guessed fallback.
// Callers that can retry with a different flame/seed (e.g. random batch
// generation) use this to detect and discard flames that would otherwise
// render blank; other callers are free to ignore the return value.
bool autoFrameFlame(Flame& flame, std::uint64_t seed, int minValidSamples = kDefaultMinValidSamples);

} // namespace apo
