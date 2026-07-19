#pragma once

#include <cstdint>
#include <limits>

namespace apo {

// Peak Signal-to-Noise Ratio (dB) between two same-sized 8-bit images, each
// row-major/top-to-bottom with `channels` bytes per pixel. Higher is more
// similar; identical images return +infinity (MSE = 0).
//
// Built for Phase 5's correctness harness, per the migration plan's own
// guidance: "exact pixel match isn't the bar... compare via a
// perceptual/histogram metric (e.g. PSNR or SSIM above a threshold) rather
// than bitwise equality" - written with two different uses in mind:
//   - Self-regression: comparing a fresh single-threaded (deterministic)
//     render against a checked-in baseline PNG, where output should be
//     bit-identical run-to-run on the same build, so a *very* high PSNR
//     threshold (see tests/baseline_regression_test.cpp) still catches
//     real drift while tolerating e.g. a different libpng/compiler
//     producing a handful of off-by-one channel values.
//   - Golden-image comparison against a real Apophysis/Delphi render
//     (the plan's originally-envisioned use, still blocked on having an
//     actual Delphi build available - see OPTIMIZATION_STATUS.md and the
//     Phase 4 summary) - there, multithreaded RNG streams differ by
//     design (Fix #1's accepted tradeoff, carried into this port's own
//     Rng.h), so only a much looser threshold would ever be appropriate,
//     and only for a single-threaded-vs-single-threaded comparison in the
//     first place.
//
// Returns -1 (an impossible PSNR value, since real PSNR is always >= 0)
// if `width`/`height`/`channels` don't describe a valid nonempty image -
// callers should treat that as "not comparable", not "identical" or
// "maximally different".
double computePsnr(int width, int height, int channels, const std::uint8_t* a, const std::uint8_t* b);

} // namespace apo
