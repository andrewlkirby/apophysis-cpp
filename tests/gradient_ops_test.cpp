// Tests for the Adjust dialog's Gradient-tab math (src/core/edit/
// GradientOps.h) - kept Qt-free and unit-tested independently of the
// dialog itself, mirroring triangle_geometry_test.cpp's approach for the
// Editor window.

#include <cmath>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/Rng.h"
#include "core/edit/GradientOps.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

apo::ColorMap makeRampGradient() {
    // entries[i] = (i, 255-i, 128) - deterministic, non-constant on every
    // channel, easy to reason about by hand for each transform below.
    apo::ColorMap cmap{};
    for (int i = 0; i < 256; ++i) {
        cmap.entries[i][0] = i;
        cmap.entries[i][1] = 255 - i;
        cmap.entries[i][2] = 128;
        cmap.entries[i][3] = 255;
    }
    return cmap;
}

void testRotateGradient() {
    const apo::ColorMap ramp = makeRampGradient();
    const apo::ColorMap rotated = apo::rotateGradient(ramp, 10);
    check(rotated.entries[10][0] == ramp.entries[0][0], "rotateGradient(10): entry 10 takes entry 0's old value");
    check(rotated.entries[0][0] == ramp.entries[246][0], "rotateGradient(10): entry 0 wraps around to entry 246's old value");

    const apo::ColorMap identity = apo::rotateGradient(ramp, 0);
    check(identity.entries[0][0] == ramp.entries[0][0] && identity.entries[255][2] == ramp.entries[255][2],
          "rotateGradient(0) is a no-op");
}

void testInvertGradient() {
    const apo::ColorMap ramp = makeRampGradient();
    const apo::ColorMap inverted = apo::invertGradient(ramp);
    check(inverted.entries[0][0] == 255 && inverted.entries[0][1] == 0,
          "invertGradient: entry 0's channels 0 and 1 (0 and 255) invert to (255 and 0)");
    check(inverted.entries[100][0] == 255 - 100 && inverted.entries[100][1] == 255 - (255 - 100),
          "invertGradient inverts every channel at an arbitrary index");
}

void testReverseGradient() {
    const apo::ColorMap ramp = makeRampGradient();
    const apo::ColorMap reversed = apo::reverseGradient(ramp);
    check(reversed.entries[0][0] == ramp.entries[255][0] && reversed.entries[255][0] == ramp.entries[0][0],
          "reverseGradient swaps entry 0 and entry 255");
    check(reversed.entries[100][0] == ramp.entries[155][0], "reverseGradient mirrors an arbitrary interior index");
}

void testBrightnessShiftClampsAtBounds() {
    const apo::ColorMap ramp = makeRampGradient();
    const apo::ColorMap brighter = apo::brightnessShiftGradient(ramp, 50);
    check(brighter.entries[0][0] == 50, "brightnessShiftGradient adds delta to a mid-range channel value");
    check(brighter.entries[220][0] == 255, "brightnessShiftGradient clamps at 255 rather than overflowing");

    const apo::ColorMap darker = apo::brightnessShiftGradient(ramp, -300);
    check(darker.entries[255][0] == 0, "brightnessShiftGradient clamps at 0 for a large negative delta");
}

void testContrastShiftPushesAwayFromMidpoint() {
    const apo::ColorMap ramp = makeRampGradient();
    // Channel 0 at index 200 is 200 (> 127) - positive contrast should push
    // it further from 127, i.e. increase it (clamped at 255).
    const apo::ColorMap contrasted = apo::contrastShiftGradient(ramp, 50);
    check(contrasted.entries[200][0] >= ramp.entries[200][0],
          "positive contrast pushes an above-midpoint value further from the midpoint");
    check(contrasted.entries[50][0] <= ramp.entries[50][0],
          "positive contrast pushes a below-midpoint value further from the midpoint (downward)");

    // Zero amount is a no-op (intens=0 makes the update term vanish).
    const apo::ColorMap noOp = apo::contrastShiftGradient(ramp, 0);
    check(noOp.entries[123][0] == ramp.entries[123][0], "contrastShiftGradient(0) is a no-op");
}

void testHueShiftFullRotationIsNoOp() {
    const apo::ColorMap ramp = makeRampGradient();
    const apo::ColorMap shifted = apo::hueShiftGradient(ramp, 360.0);
    // A full 360-degree hue rotation should return (within rounding) to the
    // original color at every saturated entry.
    bool allClose = true;
    for (int i = 0; i < 256; ++i) {
        for (int ch = 0; ch < 3; ++ch) {
            if (std::abs(shifted.entries[i][ch] - ramp.entries[i][ch]) > 2) allClose = false;
        }
    }
    check(allClose, "a full 360-degree hueShiftGradient returns (within rounding) to the original colors");
}

void testSaturationShiftLeavesGrayUntouched() {
    apo::ColorMap gray{};
    for (int i = 0; i < 256; ++i) {
        gray.entries[i] = {128, 128, 128, 255};
    }
    const apo::ColorMap shifted = apo::saturationShiftGradient(gray, 100);
    check(shifted.entries[0][0] == 128 && shifted.entries[0][1] == 128 && shifted.entries[0][2] == 128,
          "saturationShiftGradient leaves already-gray (fully desaturated) entries untouched");
}

void testBlurGradientAveragesNeighborsExcludingSelf() {
    apo::ColorMap cmap{};
    // A single bright spike at index 128, everything else 0 - blurring with
    // radius 1 should average exactly the two immediate neighbors (which
    // are both 0), leaving the spike's own blurred value at 0 too (self is
    // excluded from the average).
    cmap.entries[128] = {200, 0, 0, 255};
    const apo::ColorMap blurred = apo::blurGradient(cmap, 1);
    check(blurred.entries[128][0] == 0, "blurGradient excludes the entry itself from its own average");
    check(blurred.entries[127][0] == 100 && blurred.entries[129][0] == 100,
          "blurGradient(radius=1) averages a spike's two immediate neighbors (spike + 0)/2 = 100");

    const apo::ColorMap identity = apo::blurGradient(cmap, 0);
    check(identity.entries[128][0] == 200, "blurGradient(radius=0) is a no-op");
}

void testFrequencyGradientRepeatsCompressedPalette() {
    const apo::ColorMap ramp = makeRampGradient();
    const apo::ColorMap doubled = apo::frequencyGradient(ramp, 2);
    // times=2 -> n=128; entries [0..128) get a copy of ramp[0], ramp[2],
    // ramp[4], ... (i.e. entries[i] = ramp[i*2]), repeated across both halves.
    check(doubled.entries[0][0] == ramp.entries[0][0], "frequencyGradient(2): first entry unchanged");
    check(doubled.entries[10][0] == ramp.entries[20][0], "frequencyGradient(2): compresses the source by 2x");

    const apo::ColorMap identity = apo::frequencyGradient(ramp, 1);
    check(identity.entries[77][0] == ramp.entries[77][0], "frequencyGradient(1) is a no-op");
}

void testRandomGradientIsDeterministicPerSeedAndVariesAcrossSeeds() {
    apo::Xoshiro256Rng rngA(42);
    apo::Xoshiro256Rng rngB(42);
    const apo::ColorMap a = apo::randomGradient(rngA);
    const apo::ColorMap b = apo::randomGradient(rngB);
    check(a.entries == b.entries, "randomGradient is deterministic for a fixed seed");

    apo::Xoshiro256Rng rngC(1234);
    const apo::ColorMap c = apo::randomGradient(rngC);
    check(!(a.entries == c.entries), "randomGradient produces different output for a different seed");
}

void testFlameAdjustScaleRescalesPixelsPerUnitProportionally() {
    apo::Flame flame;
    flame.width = 100;
    flame.height = 100;
    flame.pixelsPerUnit = 50;

    flame.adjustScale(200, 150);
    check(flame.width == 200 && flame.height == 150, "adjustScale updates width/height to the requested size");
    check(approxEqual(flame.pixelsPerUnit, 100.0), "adjustScale doubles pixelsPerUnit when width doubles (100->200)");
}

} // namespace

int main() {
    testRotateGradient();
    testInvertGradient();
    testReverseGradient();
    testBrightnessShiftClampsAtBounds();
    testContrastShiftPushesAwayFromMidpoint();
    testHueShiftFullRotationIsNoOp();
    testSaturationShiftLeavesGrayUntouched();
    testBlurGradientAveragesNeighborsExcludingSelf();
    testFrequencyGradientRepeatsCompressedPalette();
    testRandomGradientIsDeterministicPerSeedAndVariesAcrossSeeds();
    testFlameAdjustScaleRescalesPixelsPerUnitProportionally();
    return apo_test::reportAndExit();
}
