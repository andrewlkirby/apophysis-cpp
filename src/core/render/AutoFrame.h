#pragma once

#include <cstdint>

#include "../Flame.h"

namespace apo {

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
// Returns false if the attractor was too degenerate to frame at all (fewer
// than 100 of the sampled points landed anywhere sensible) - center/
// pixelsPerUnit are left untouched in that case. Callers that can retry
// with a different flame/seed (e.g. random batch generation) can use this
// to detect and discard flames that would otherwise render blank; other
// callers are free to ignore the return value, matching prior behavior.
bool autoFrameFlame(Flame& flame, std::uint64_t seed);

} // namespace apo
