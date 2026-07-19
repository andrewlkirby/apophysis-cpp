#pragma once

#include <cstdint>

#include "core/ColorMap.h"
#include "core/Rng.h"

namespace apo {

// Options.pas's Smooth Palette settings (NumTries/TryLength) - defaults
// match Regstry.pas's own fallbacks exactly (NumTries=10, TryLength=100000,
// used whenever no saved registry value exists yet).
struct SmoothPaletteParams {
    int numTries = 10;
    int tryLength = 100000;
};

// A5 - derives a smooth 256-entry gradient from an arbitrary image, porting
// Draves' "Smooth palette" GIMP plug-in algorithm exactly as Main.pas:4165's
// TMainForm.SmoothPalette does: sample 256 random pixels from the image,
// then run `numTries` independent scramble-and-locally-optimize passes
// (each: randomly shuffle the 256 samples, then `tryLength` random-pair
// swap-if-it-locally-improves-adjacent-similarity iterations), keeping
// whichever pass ended with the lowest total adjacent-squared-RGB-distance,
// followed by a short "clean" pass of 1024 more such improving swaps. The
// result is a random sample of the image's own colors, reordered into the
// smoothest-looking traversal a stochastic search can find within the
// tries/length budget - not an average or a sorted-by-hue palette, this is
// the actual, if unusual, algorithm the original's own name for it refers
// to.
//
// `pixelsRgb` is `width*height*3` bytes, row-major top-to-bottom, 3 bytes
// per pixel (R,G,B) - deliberately QImage-free (Qt-free core code, matching
// every other core/edit/ function - see GradientOps.h's own doc comment):
// the UI layer decodes whatever image format via QImage and hands this
// function a plain RGB buffer. `rng` is caller-supplied (this port's
// established per-call-site Rng ownership, not a hidden global stream -
// see Rng.h's own class comment on why the original's direct global
// Random() calls were deliberately not carried over).
//
// Fixes two array-bounds bugs in the original, not replicated:
//   - The "measure" step's `for i := 0 to 255 do len := len +
//     diffcolor(clist, i, i + 1)` reads clist[256] at i=255, one past the
//     256-entry array - fixed here by summing i in [0,254] (255 adjacent
//     pairs across a 256-entry chain, the well-defined quantity the loop
//     was actually trying to compute).
//   - The "clean" step's `i0 := 1 + random(254)` (range [1,254]) then
//     `diffcolor(clist, i1, i1 + 1)` with i1=i0+1 reads clist[256] again
//     when i0=254 - fixed here by drawing i0 from [1,253] instead, so
//     i1+1 (at most 255) always stays in bounds.
// Both are genuine out-of-bounds reads in the Delphi source (undefined
// behavior, not a deliberate design choice) - per the "good performance,
// not a faithful copy" project direction, fixed rather than reproduced.
//
// Returns false (leaving `out` unmodified) if width/height aren't positive
// or pixelsRgb is null.
bool smoothPaletteFromImage(const std::uint8_t* pixelsRgb, int width, int height, Rng& rng,
                             const SmoothPaletteParams& params, ColorMap& out);

} // namespace apo
