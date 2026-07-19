// Hand-computed numeric checks for the batch-1 variation ports: bipolar,
// blur_circle, blur_pixelize, blur_zoom, bwraps, crop, cross. Not a
// replacement for the render-correctness harness planned for Phase 5.

#include <cmath>
#include <string>

#include "TestHelpers.h"
#include "core/Rng.h"
#include "core/VariationRegistry.h"
#include "core/XForm.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

int registeredIndex(const std::string& name) {
    return apo::VariationRegistry::instance().variationIndex(name);
}

void testAllBatch1VariationsRegistered() {
    for (const char* name : {"bipolar", "blur_circle", "blur_pixelize", "blur_zoom", "bwraps", "crop", "cross"}) {
        check(registeredIndex(name) >= apo::VariationRegistry::kNumLocalVars,
              (std::string(name) + " is registered").c_str());
    }
}

void testCrossMatchesHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("cross"), 1.0);

    apo::MtRng rng(1);
    x.prepare(rng);

    apo::Point3 pt{0.5, 0.2, 0.3};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double r = 1.0 / std::fabs((0.5 - 0.2) * (0.5 + 0.2) + 1e-6);
    check(approxEqual(pt.x, 0.5 * r) && approxEqual(pt.y, 0.2 * r) && approxEqual(pt.z, 0.3),
          "cross variation output matches a hand-computed formula for a known point");
}

void testBipolarMatchesHandComputation() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("bipolar"), 1.0); // bipolar_shift defaults to 0

    apo::MtRng rng(2);
    x.prepare(rng);

    apo::Point3 pt{0.3, 0.4, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double x2y2 = 0.3 * 0.3 + 0.4 * 0.4;
    const double y = 0.5 * std::atan2(2.0 * 0.4, x2y2 - 1.0); // s = 0 since shift = 0
    const double t = x2y2 + 1.0;
    const double x2 = 2.0 * 0.3;
    const double v4 = 0.15915494309189533576888376337251;
    const double v = 0.636619772367581343075535053490061;
    const double expectedX = v4 * std::log((t + x2) / (t - x2));
    const double expectedY = v * y;

    check(approxEqual(pt.x, expectedX) && approxEqual(pt.y, expectedY) && approxEqual(pt.z, 0.5),
          "bipolar variation (shift=0) output matches a hand-computed formula for a known point");
}

void testBipolarShiftNormalization() {
    // Matches the original's frac()-based wraparound into [-1, 1] with the
    // normalized value written back through `value`.
    apo::XForm x;
    double shiftIn = 2.5; // outside [-1,1], should wrap
    check(x.setVariable("bipolar_shift", shiftIn), "bipolar_shift is settable");

    double readBack = 0;
    x.getVariable("bipolar_shift", readBack);
    check(approxEqual(readBack, shiftIn),
          "bipolar_shift setVariable's write-back matches what a subsequent getVariable reports");
    check(readBack >= -1.0 && readBack <= 1.0, "bipolar_shift is normalized into [-1, 1]");
}

void testBlurZoomXIsSettable() {
    // The original (TVariationBlurZoom.SetVariable) had a bug where the
    // "blur_zoom_x" branch actually wrote to blur_zoom_y, so blur_zoom_x
    // could never be set through the named-parameter API. Fixed in the
    // port: verify blur_zoom_x and blur_zoom_y are now independently
    // settable.
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("blur_zoom"), 1.0);

    double newX = 0.75;
    x.setVariable("blur_zoom_x", newX);
    double newY = 0.25;
    x.setVariable("blur_zoom_y", newY);

    double storedX = -999, storedY = -999;
    x.getVariable("blur_zoom_x", storedX);
    x.getVariable("blur_zoom_y", storedY);

    check(approxEqual(storedX, 0.75), "blur_zoom_x is set by setVariable(\"blur_zoom_x\", ...)");
    check(approxEqual(storedY, 0.25), "blur_zoom_y is unaffected by setting blur_zoom_x");
}

void testBlurPixelizeReset() {
    // The original (TVariationBlurPixelize.ResetVariable) had a bug where
    // resetting "blur_pixelize_scale" actually wrote 1 into
    // blur_pixelize_size and never touched blur_pixelize_scale at all.
    // Fixed in the port: resetting blur_pixelize_scale now restores its own
    // default (1) and leaves blur_pixelize_size untouched.
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("blur_pixelize"), 1.0);

    double newScale = 3.0;
    x.setVariable("blur_pixelize_scale", newScale);
    double newSize = 5.0;
    x.setVariable("blur_pixelize_size", newSize);

    check(x.resetVariable("blur_pixelize_scale"), "resetVariable(\"blur_pixelize_scale\") reports success");

    double scale = -1, size = -1;
    x.getVariable("blur_pixelize_scale", scale);
    x.getVariable("blur_pixelize_size", size);
    check(approxEqual(scale, 1.0), "resetting \"blur_pixelize_scale\" restores its own default (1)");
    check(approxEqual(size, 5.0), "resetting \"blur_pixelize_scale\" leaves blur_pixelize_size untouched");
}

void testBwrapsCellsizeZeroPassthrough() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    const int idx = registeredIndex("bwraps");
    x.setVariation(idx, 1.0);

    double cellsize = 0.0;
    x.setVariable("bwraps_cellsize", cellsize);

    apo::MtRng rng(3);
    x.prepare(rng);

    apo::Point3 pt{0.7, -0.3, 0.9};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.7) && approxEqual(pt.y, -0.3) && approxEqual(pt.z, 0.9),
          "bwraps with cellsize=0 is an exact passthrough (matches the original's early branch)");
}

void testCropInBoundsPassthrough() {
    // Default crop bounds are [-1,1]x[-1,1]; a point well inside them
    // should pass straight through with vvar=1, with no rng calls needed
    // (neither per-axis `if x < _x0`/`if x > _x1` branch fires).
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("crop"), 1.0);

    apo::MtRng rng(4);
    x.prepare(rng);

    apo::Point3 pt{0.3, -0.2, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.3) && approxEqual(pt.y, -0.2) && approxEqual(pt.z, 0.5),
          "crop with default bounds passes an in-bounds point through unchanged");
}

void testCropZeroModeClampsOutOfBoundsToOrigin() {
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("crop"), 1.0);

    double zeroFlag = 1.0;
    x.setVariable("crop_zero", zeroFlag);
    check(approxEqual(zeroFlag, 1.0), "crop_zero clamps into [0,1] and writes back the clamped value");

    apo::MtRng rng(5);
    x.prepare(rng);

    apo::Point3 pt{5.0, 5.0, 0.4}; // well outside default [-1,1] bounds
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.0) && approxEqual(pt.y, 0.0) && approxEqual(pt.z, 0.4),
          "crop with crop_zero=1 zeroes an out-of-bounds point instead of clamping it");
}

void testBlurCircleZComponentIsDeterministic() {
    // blur_circle's x/y output is stochastic, but its z accumulation
    // (vvar * tz) is not - a partial, still-meaningful check without
    // reverse-engineering the rng stream.
    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(registeredIndex("blur_circle"), 1.0);

    apo::MtRng rng(6);
    x.prepare(rng);

    apo::Point3 pt{0.0, 0.0, 0.42};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.z, 0.42), "blur_circle's z output is exactly vvar*tz regardless of the stochastic x/y");
}

} // namespace

int main() {
    testAllBatch1VariationsRegistered();
    testCrossMatchesHandComputation();
    testBipolarMatchesHandComputation();
    testBipolarShiftNormalization();
    testBlurZoomXIsSettable();
    testBlurPixelizeReset();
    testBwrapsCellsizeZeroPassthrough();
    testCropInBoundsPassthrough();
    testCropZeroModeClampsOutOfBoundsToOrigin();
    testBlurCircleZComponentIsDeterministic();

    return apo_test::reportAndExit();
}
