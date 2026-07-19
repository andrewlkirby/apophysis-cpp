#pragma once

#include "core/ColorMap.h"
#include "core/Rng.h"

namespace apo {

// Pure gradient-editing transforms for the Adjust dialog's Gradient tab
// (Adjust.pas's ScrollBarChange scrollMode cases + mnuInvert/mnuReverse/
// mnuRandomize). Qt-free and independently unit-tested (see
// tests/gradient_ops_test.cpp) so the UI layer only has to wire slider
// values to these functions and paint the result, mirroring how
// TriangleGeometry.h keeps the Editor's drag math out of TriangleCanvas.

// Rotates the gradient by `offset` positions - output entry i takes the
// value previously at (i - offset) mod 256. Matches Adjust.pas's
// modeRotate (UI slider range -128..128, but any integer works via modular
// wraparound).
ColorMap rotateGradient(const ColorMap& in, int offset);

// Shifts every entry's hue by `degrees` (UI slider range -180..180).
// Matches Adjust.pas's modeHue. Hue is mathematically inert on a
// fully-desaturated (gray) entry regardless - no special-casing needed.
ColorMap hueShiftGradient(const ColorMap& in, double degrees);

// Adds `delta` percentage points (UI slider range -100..100) to every
// entry's saturation, clamped to 0..100. Entries that are already fully
// desaturated (gray) are left untouched - matches Adjust.pas's
// modeSaturation, which otherwise would turn gray entries an arbitrary hue
// (whatever the hue channel degenerates to when s=0) the instant
// saturation is added back in.
ColorMap saturationShiftGradient(const ColorMap& in, double delta);

// Adds `delta` (UI slider range -255..255) to every RGB channel, clamped
// to 0..255. Matches Adjust.pas's modeBrightness.
ColorMap brightnessShiftGradient(const ColorMap& in, int delta);

// Adjusts contrast by `amount` (UI slider range -100..100) around the
// midpoint (127). Matches Adjust.pas's modeContrast, including its
// asymmetric feel: positive amounts are doubled before applying.
ColorMap contrastShiftGradient(const ColorMap& in, int amount);

// Averages each entry with its `radius` neighbors on each side (excluding
// itself), matching Adjust.pas's Blur - radius=0 is a no-op. Divides by
// the actual neighbor count (2*radius), not 2*radius+1: the original
// divides by the total loop-iteration count including the skipped
// self-entry, which biases every blurred value slightly toward zero. That
// skew serves no visual purpose here, so this fixes it rather than
// reproducing it (per the "good performance, not a faithful copy"
// directive) - the exact denominator of a cosmetic gradient-blur slider
// isn't behavior anything else depends on.
ColorMap blurGradient(const ColorMap& in, int radius);

// Repeats the first (256/times) entries `times` times across the full
// gradient - matches Adjust.pas's Frequency. times=1 is a no-op.
ColorMap frequencyGradient(const ColorMap& in, int times);

// Inverts every channel (255 - value).
ColorMap invertGradient(const ColorMap& in);

// Reverses the entry order.
ColorMap reverseGradient(const ColorMap& in);

// Generates a new gradient from a handful of random HSV control points
// linearly blended together - matches the spirit of GradientHlpr.pas's
// RandomGradient. The node-count/hue/saturation/luminance ranges were
// separate user-configurable Options-dialog fields in the original (not
// yet ported - Options is a Phase 7 dialog); this hardcodes sensible
// defaults instead of wiring up that dialog just for this one feature.
ColorMap randomGradient(Rng& rng);

} // namespace apo
