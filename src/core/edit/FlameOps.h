#pragma once

#include "../Flame.h"
#include "../Rng.h"

// A6 - Main.pas's Flame-menu weight/color utility commands (Random
// Weights/Equalize Weights/Calculate Colors Values/Randomize Color
// Values), ported as small pure Qt-free functions (mirrors GradientOps.h's
// role for the Adjust dialog's gradient tab). Each operates only on the
// flame's currently-active xforms (numXForms()), matching the original's
// own `Transforms` (active count, excluding the final xform) scope
// exactly - none of these ever touch the final xform.
//
// NOT ported: Main.dfm's "Compute Weights" (mnuNormalWeights). Verified
// directly against Main.pas: mnuNormalWeightsClick's own real call
// (`ComputeWeights(MainCp, MainTriangles, transforms)`) is commented out
// behind a `// TODO: ...something`, so the menu item is dead in the
// original itself - clicking it does nothing to the flame beyond the
// generic StopThread/UpdateUndo/redraw bookkeeping every menu handler
// does. The underlying TControlPoint.ComputeWeights it would have called
// is also a dead end worth skipping on its own merits, not just because
// it's unreachable: its triangle_area() helper (Global.pas) has a
// degenerate-case branch (`if base < 1.0 then Result := height`) that
// returns a length where an area is expected whenever a transform's own
// image-triangle is smaller than one unit - which is the common case for
// a typical contractive IFS xform, not a rare edge case. That's not a
// clean formula worth fixing-and-porting, just a dimensionally broken
// heuristic sitting behind an already-dead menu item - per the plan's own
// "verify each formula against Main.pas before porting" instruction, this
// is exactly a case where the paraphrase ("Compute Weights") promised a
// working feature that investigation shows isn't one.
namespace apo {

// Sets every active xform's density (weight) to a uniform random value in
// [0, 1) - matches ControlPoint.pas's RandomizeWeights exactly.
void randomizeXformWeights(Flame& flame, Rng& rng);

// Sets every active xform's density to a fixed 0.5 - matches
// ControlPoint.pas's EqualizeWeights exactly (a literal constant, not
// 1/numXForms: the renderer normalizes by total density when building its
// propTable regardless of the shared constant's actual value, so this is
// behaviorally equivalent to any other equal-weight choice - 0.5 is simply
// what the original happened to pick).
void equalizeXformWeights(Flame& flame);

// Spreads every active xform's color index evenly across [0, 1] - xform i
// gets i/(numXForms-1) - matches Main.pas's mnuCalculateColorsClick
// exactly. A single-xform flame (numXForms<=1) leaves colors untouched
// rather than dividing by zero (the original's own `i / (transforms - 1)`
// is a real division by zero there when Transforms=1 - fixed here, not
// replicated, per the "good performance, not a faithful copy" direction).
void calculateXformColorValues(Flame& flame);

// Sets every active xform's color index to a uniform random value in
// [0, 1) - matches Main.pas's mnuRandomizeColorValuesClick exactly.
void randomizeXformColorValues(Flame& flame, Rng& rng);

} // namespace apo
