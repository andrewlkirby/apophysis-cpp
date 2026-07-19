#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace apo {

// Mirrors Delphi's TColorMap (ColorMap/cmap.pas): a 256-entry RGBA palette.
// Channel order/range matches the original: [0]=R [1]=G [2]=B [3]=A (unused
// by the renderer, kept for layout parity with ParameterIO's XML), each 0-255.
struct ColorMap {
    std::array<std::array<int, 4>, 256> entries{};
};

void rgbToHsv(const double rgb[3], double hsv[3]);
void hsvToRgb(const double hsv[3], double rgb[3]);

// Applies a hue rotation (0..1, matching cmap.pas's `hue_rotation` - a full
// rotation is 1.0, internally scaled by 6 to match GetCmap's `hsv[0] +=
// hue_rotation * 6`) to every entry of `in`, writing the result to `out`.
// `out` may alias `in`.
void applyHueRotation(const ColorMap& in, double hueRotation, ColorMap& out);

// Parses a gradient-string in Apophysis's brace-delimited format
// (`{ ... index=N color=RRGGBB-as-decimal-int ... }`), matching
// cmap.pas's GetPalette. Returns false (and leaves `out` unmodified) on a
// malformed string - same failure contract as the original (missing
// braces, no color entries, or an empty index list).
//
// NOTE: this does not yet cover the ~701 built-in gradients from
// ColorMap/cmapdata.pas (2.8MB of generated constant data in the original -
// not hand-ported; a follow-up generation script should translate that file
// mechanically rather than by hand). Only gradient-string parsing (used for
// .flame/.flam3 XML `<colors>` entries and standalone .map/.ugr files) is
// covered here.
bool parseGradientString(const std::string& gradientText, ColorMap& out);

// Linearly interpolates palette entries strictly between indices `a` and
// `b` (exclusive on both ends), per channel, wrapping through 256 - matches
// cmap.pas's RGBBlend. Used by parseGradientString's index-based gradient
// strings and by GradientOps.h's randomGradient to fill the gaps between a
// handful of randomly-placed control-point colors.
void rgbBlendBetween(int a, int b, ColorMap& palette);

} // namespace apo
