#pragma once

#include "../Flame.h"

namespace apo {

// Port of ControlPoint.pas's add_symmetry_to_control_point - appends
// xforms implementing rotational and/or reflective (dihedral) symmetry to
// `flame`, on top of whatever xforms it already has. In the original this
// is only ever invoked as a random-flame-generation post-processing step
// (Main.pas/RndFlame.pas's SymmetryType/SymmetryOrder options); this port
// exposes it as a direct, on-demand Editor action instead (see
// src/ui/ForceSymmetryDialog.h) - a "good performance, not a faithful
// copy" enhancement: the same useful math, made directly reachable on an
// already-open flame rather than buried in a generation-time-only option.
//
//   sym == 0 or sym == 1: no-op, returns 0 (matches the original's own
//     early-out - order-1 rotational symmetry is just the identity).
//   sym > 1: pure rotational symmetry of that order - appends (sym-1)
//     rotation xforms at angles 2*pi*k/sym, k=1..sym-1.
//   sym == -1: appends exactly one reflection xform (a horizontal-flip
//     affine) and no rotations.
//   sym < -1: appends one reflection xform plus (|sym|-1) rotation
//     xforms - the combination produces dihedral (mirror) symmetry of
//     order |sym|.
//
// Every appended xform is pure-linear, density 1, and symmetry=1 (pinned
// against MutateDialog's own random reshaping - matches the original's
// `.symmetry := 1` on every xform it adds here).
//
// Deliberately does not port the original's separately-configurable
// SymmetryNVars safety cap (Options.pas, default 12) on how many xforms a
// single call may append - this port bounds against Flame::kMaxXForms
// directly (the array's actual capacity), which is simpler, still fully
// safe against overflow, and more permissive than the original's
// conservative default.
//
// Returns the number of xforms actually appended (0 if the flame was
// already at kMaxXForms, or sym was a no-op value).
int addSymmetry(Flame& flame, int sym);

} // namespace apo
