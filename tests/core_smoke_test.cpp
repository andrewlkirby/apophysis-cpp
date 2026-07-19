// Zero-dependency smoke tests for Phase 1 (core data model) and the first
// hand-ported Phase 2 variations. Not a replacement for the render-
// correctness harness planned for Phase 5 (reference-image diffing against
// a golden Delphi build) - this only checks that the C++ port's basic math
// matches hand-computed expectations for a few simple, verifiable cases.

#include <cmath>
#include <string>

#include "TestHelpers.h"
#include "core/ColorMap.h"
#include "core/Flame.h"
#include "core/Rng.h"
#include "core/VariationRegistry.h"
#include "core/XForm.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

void testIdentityXFormIsIdentity() {
    apo::XForm x; // default: vars[0] (linear) = 1, all coefs identity
    apo::MtRng rng(1234);
    x.prepare(rng);

    apo::Point3 pt{1.0, 2.0, 3.0};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 1.0) && approxEqual(pt.y, 2.0) && approxEqual(pt.z, 3.0),
          "identity XForm (linear=1, identity affine) leaves a point unchanged");
}

void testOpacityAlwaysPassesReflectsTransOpacity() {
    // FOLLOWUP_PLAN.txt B3: Renderer.cpp's per-point opacity roll skips
    // drawing from the RNG entirely when this is true - prepare() must
    // recompute it every call (not just at construction), since transOpacity
    // is a plain settable field a caller can change between prepare() calls
    // (e.g. MainWindow's soloXform handling forces transOpacity=0 on every
    // non-solo xform right before calling prepare()).
    apo::XForm x;
    apo::MtRng rng(1);

    x.prepare(rng);
    check(x.opacityAlwaysPasses(), "a freshly constructed XForm's default transOpacity (1) always passes");

    x.transOpacity = 0.5;
    x.prepare(rng);
    check(!x.opacityAlwaysPasses(), "transOpacity < 1 does not always pass");

    x.transOpacity = 0.0;
    x.prepare(rng);
    check(!x.opacityAlwaysPasses(), "transOpacity == 0 does not always pass either - only exactly 1.0 does");

    x.transOpacity = 1.0;
    x.prepare(rng);
    check(x.opacityAlwaysPasses(), "restoring transOpacity to exactly 1.0 and re-preparing flips it back to true");
}

void testRotate90Degrees() {
    apo::XForm x;
    x.rotate(90.0);

    check(approxEqual(x.c[0][0], 0.0) && approxEqual(x.c[0][1], -1.0) &&
              approxEqual(x.c[1][0], 1.0) && approxEqual(x.c[1][1], 0.0) &&
              approxEqual(x.c[2][0], 0.0) && approxEqual(x.c[2][1], 0.0),
          "rotate(90) on an identity XForm produces a 90-degree rotation matrix");
}

void testVariationRegistryLocalNames() {
    const auto& names = apo::VariationRegistry::localVarNames();
    check(names.size() == 29, "VariationRegistry has exactly NRLOCVAR (29) local variation names");
    check(names[0] == "linear", "local variation 0 is 'linear'");
    check(names[10] == "diamond", "local variation 10 (the Square procedure) is named 'diamond'");
    check(names[28] == "post_rotate_y", "local variation 28 is 'post_rotate_y'");
}

void testGradientParsing() {
    const std::string gradient =
        "Test Gradient {\n"
        "index=0 color=255\n"
        "index=399 color=16711680\n"
        "}\n";

    apo::ColorMap map;
    const bool ok = apo::parseGradientString(gradient, map);
    check(ok, "parseGradientString succeeds on a well-formed 2-stop gradient");
    if (!ok) return;

    check(map.entries[0][0] == 255 && map.entries[0][1] == 0 && map.entries[0][2] == 0,
          "gradient stop at index=0 parses as red (255,0,0)");
    check(map.entries[255][0] == 0 && map.entries[255][1] == 0 && map.entries[255][2] == 255,
          "gradient stop at index=399 (normalized to 255) parses as blue (0,0,255)");
    check(map.entries[128][0] > 100 && map.entries[128][0] < 150,
          "midpoint entry is blended (red channel roughly halfway down)");

    apo::ColorMap malformed;
    check(!apo::parseGradientString("not a gradient", malformed),
          "parseGradientString rejects text with no closing brace");
}

void testFlameDefaults() {
    apo::Flame flame;
    check(flame.numXForms() == 0, "a freshly constructed Flame has zero active xforms");
    check(flame.xform[0]->variation(0) == 1.0, "each xform defaults to linear=1 (matches XForm::clear())");
}

void testAddXFormAppendsDefaultAndIsReachable() {
    apo::Flame flame;
    flame.xform[0]->density = 1.0;
    flame.xform[0]->modWeights[1] = 0.75; // pre-set weight toward the not-yet-existing slot 1

    const int idx = flame.addXForm();
    check(idx == 1, "addXForm on a 1-xform flame appends at index 1");
    check(flame.numXForms() == 2, "numXForms reflects the newly appended xform");
    check(approxEqual(flame.xform[1]->density, 0.5), "a freshly added xform defaults to density 0.5");
    check(approxEqual(flame.xform[1]->variation(0), 1.0), "a freshly added xform defaults to pure linear");
    check(approxEqual(flame.xform[0]->modWeights[1], 0.75),
          "addXForm doesn't touch other xforms' xaos weights toward the new slot - it was already reachable "
          "by whatever weight (or the default) that slot already held");

    apo::Flame full;
    for (int i = 0; i < apo::Flame::kMaxXForms; ++i) full.xform[i]->density = 1.0;
    check(full.addXForm() == -1, "addXForm on an already-full flame (kMaxXForms active) is a no-op returning -1");
}

void testDuplicateXFormCopiesXaosLinksInBothDirections() {
    apo::Flame flame;
    flame.xform[0]->density = 1.0;
    flame.xform[0]->color = 0.25;
    flame.xform[1]->density = 1.0;
    flame.xform[1]->color = 0.75;
    flame.xform[0]->modWeights[1] = 0.4; // 0 -> 1
    flame.xform[1]->modWeights[0] = 0.6; // 1 -> 0
    flame.xform[1]->modWeights[1] = 0.9; // 1 -> self

    const int idx = flame.duplicateXForm(1);
    check(idx == 2, "duplicateXForm appends the copy at the next free index");
    check(approxEqual(flame.xform[2]->color, 0.75), "the duplicate is a full copy of the source xform's own fields");
    check(approxEqual(flame.xform[0]->modWeights[2], flame.xform[0]->modWeights[1]),
          "every other xform's weight toward the duplicate matches its weight toward the original");
    check(approxEqual(flame.xform[2]->modWeights[0], flame.xform[1]->modWeights[0]),
          "the duplicate's own outgoing weights match the original's (via the full-field copy)");
    check(approxEqual(flame.xform[2]->modWeights[2], flame.xform[1]->modWeights[1]),
          "the duplicate's self-weight matches the original's self-weight");

    check(flame.duplicateXForm(5) == -1, "duplicateXForm on an out-of-range index is a no-op returning -1");
}

void testRemoveXFormCompactsIndicesAndXaosColumns() {
    apo::Flame flame;
    for (int i = 0; i < 3; ++i) flame.xform[i]->density = 1.0;
    flame.xform[0]->color = 0.0;
    flame.xform[1]->color = 0.5;
    flame.xform[2]->color = 1.0;
    // A 3x3 weight matrix with distinct values so compaction is verifiable
    // element-by-element, not just "still non-default somewhere".
    flame.xform[0]->modWeights = {1.0, 2.0, 3.0};
    flame.xform[1]->modWeights = {4.0, 5.0, 6.0};
    flame.xform[2]->modWeights = {7.0, 8.0, 9.0};
    flame.soloXform = 2;

    flame.removeXForm(1); // delete the middle xform

    check(flame.numXForms() == 2, "removeXForm shrinks numXForms by one");
    check(approxEqual(flame.xform[0]->color, 0.0) && approxEqual(flame.xform[1]->color, 1.0),
          "the surviving xforms are re-packed in order (old index 2 slides down to index 1)");
    // Row 0 (xform that was at index 0, unaffected by row-compaction) should
    // have had column 1 (the deleted target) dropped and column 2 shifted
    // left to column 1: {1,2,3} -> {1,3}.
    check(approxEqual(flame.xform[0]->modWeights[0], 1.0) && approxEqual(flame.xform[0]->modWeights[1], 3.0),
          "column-compaction drops the deleted xform's weight column and shifts later columns left");
    // Row that's now at index 1 (was xform 2, {7,8,9}) should show the same
    // column-compaction applied before the row itself slid down: {7,9}.
    check(approxEqual(flame.xform[1]->modWeights[0], 7.0) && approxEqual(flame.xform[1]->modWeights[1], 9.0),
          "the row-compacted survivor's own xaos weights were column-compacted too, not just carried over raw");
    check(flame.soloXform == 1, "soloXform tracking the deleted xform's higher-indexed neighbor is decremented to match");

    apo::Flame single;
    single.xform[0]->density = 1.0;
    single.removeXForm(0);
    check(single.numXForms() == 1, "removeXForm refuses to drop the flame's last remaining xform");
}

void testRegisteredVariationsIncludeCoreExamples() {
    auto& registry = apo::VariationRegistry::instance();
    check(registry.variationIndex("auger") >= apo::VariationRegistry::kNumLocalVars, "auger is registered");
    check(registry.variationIndex("pdj") >= apo::VariationRegistry::kNumLocalVars, "pdj is registered");
    check(registry.variationIndex("julian") >= apo::VariationRegistry::kNumLocalVars, "julian is registered");
    check(registry.variationIndex("curl3D") >= apo::VariationRegistry::kNumLocalVars, "curl3D is registered");
}

void testAugerMatchesHandComputation() {
    const int augerIndex = apo::VariationRegistry::instance().variationIndex("auger");

    apo::XForm x;
    x.setVariation(0, 0.0);          // turn off linear
    x.setVariation(augerIndex, 1.0); // auger weight = 1, default params (freq=5, weight=0.5, scale=0.1, sym=0)

    apo::MtRng rng(42);
    x.prepare(rng);

    apo::Point3 pt{0.3, -0.2, 0.5};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    const double xIn = 0.3, yIn = -0.2;
    // dy uses s = sin(freq*x) (not t = sin(freq*y), which feeds dx instead -
    // easy to transpose, so this is spelled out explicitly against the
    // source rather than assumed symmetric).
    const double s = std::sin(5 * xIn);
    const double dy = yIn + 0.5 * (0.5 * 0.1 * s + std::fabs(yIn) * s);
    // sym=0, so x' = x + 0*(dx-x) = x unchanged; dx's value never matters here.

    check(approxEqual(pt.x, xIn) && approxEqual(pt.y, dy) && approxEqual(pt.z, 0.5),
          "auger variation output matches a hand-computed formula for a known point");
}

void testJulianSpecializedDispatch() {
    const int julianIndex = apo::VariationRegistry::instance().variationIndex("julian");

    apo::XForm x;
    x.setVariation(0, 0.0);
    x.setVariation(julianIndex, 1.0);
    // Force julian_power to 1 (julian_dist defaults to 1 already), which
    // should route through the CalcPower1 fast path (an exact passthrough
    // scaled by vvar), matching Delphi's GetCalcFunction selection.
    double power = 1.0;
    x.setVariable("julian_power", power);

    apo::MtRng rng(7);
    x.prepare(rng);

    apo::Point3 pt{0.6, -0.4, 0.25};
    double colorCoord = 0.0;
    x.nextPoint(pt, colorCoord);

    check(approxEqual(pt.x, 0.6) && approxEqual(pt.y, -0.4) && approxEqual(pt.z, 0.25),
          "julian with power=1, dist=1 takes the CalcPower1 passthrough fast path");
}

} // namespace

int main() {
    testIdentityXFormIsIdentity();
    testOpacityAlwaysPassesReflectsTransOpacity();
    testRotate90Degrees();
    testVariationRegistryLocalNames();
    testGradientParsing();
    testFlameDefaults();
    testAddXFormAppendsDefaultAndIsReachable();
    testDuplicateXFormCopiesXaosLinksInBothDirections();
    testRemoveXFormCompactsIndicesAndXaosColumns();
    testRegisteredVariationsIncludeCoreExamples();
    testAugerMatchesHandComputation();
    testJulianSpecializedDispatch();

    return apo_test::reportAndExit();
}
