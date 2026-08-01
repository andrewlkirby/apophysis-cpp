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

void testGetVariableReadsFactoryDefaultBeforeAnyTouch() {
    // FOLLOWUP_PLAN.txt B1(b): a registered variation's Variation instance
    // is only constructed lazily (nonzero weight, or a caller reads/writes
    // one of its named parameters) - getVariable() must still report the
    // real factory-default value for a variation that has never been
    // touched at all, not just for one already materialized by some other
    // path.
    apo::XForm x; // auger's weight is 0 (default linear=1 only) - never set, never queried before now
    double freq = -1, weight = -1, scale = -1, sym = -1;
    check(x.getVariable("auger_freq", freq) && approxEqual(freq, 5.0),
          "auger_freq reads its factory default (5) on a never-touched XForm");
    check(x.getVariable("auger_weight", weight) && approxEqual(weight, 0.5),
          "auger_weight reads its factory default (0.5) on a never-touched XForm");
    check(x.getVariable("auger_scale", scale) && approxEqual(scale, 0.1),
          "auger_scale reads its factory default (0.1) on a never-touched XForm");
    check(x.getVariable("auger_sym", sym) && approxEqual(sym, 0.0),
          "auger_sym reads its factory default (0) on a never-touched XForm");
    check(!x.getVariable("not_a_real_parameter_name", freq), "an unknown parameter name is rejected");
}

void testResetVariableOnUntouchedVariationReturnsTrueWithoutChangingDefault() {
    apo::XForm x;
    check(x.resetVariable("auger_freq"), "resetVariable on a name owned by a never-touched variation still reports success");
    double freq = -1;
    check(x.getVariable("auger_freq", freq) && approxEqual(freq, 5.0),
          "...and the value is still exactly the factory default afterward");
    check(!x.resetVariable("not_a_real_parameter_name"), "resetVariable rejects an unknown parameter name");
}

void testAssignResetsUntouchedVariationBackToDefault() {
    // The core correctness risk B1(b) introduces: if the destination
    // customized a variation the source never touched, assign() must still
    // reset the destination back to the source's (default) value - not
    // silently leave the stale customization behind just because the
    // source's slot is null.
    apo::XForm src;
    apo::XForm dst;

    double customFreq = 9.0;
    dst.setVariable("auger_freq", customFreq);
    double readBack = -1;
    dst.getVariable("auger_freq", readBack);
    check(approxEqual(readBack, 9.0), "sanity check: dst's auger_freq is actually customized before assign()");

    dst.assign(src); // src never touched auger at all

    double afterAssign = -1;
    check(dst.getVariable("auger_freq", afterAssign) && approxEqual(afterAssign, 5.0),
          "assign() from an untouched source resets a previously-customized parameter back to the factory default");
}

void testAssignCopiesACustomizedValueFromSource() {
    apo::XForm src;
    apo::XForm dst;

    double customFreq = 12.0;
    src.setVariable("auger_freq", customFreq);

    dst.assign(src);

    double value = -1;
    check(dst.getVariable("auger_freq", value) && approxEqual(value, 12.0),
          "assign() copies a source's customized parameter value onto a destination that never touched it");
}

void testInterpolateVariablesFromBlendsAgainstTrueDefaultWhenOnlyOneSideTouched() {
    apo::XForm x1; // never touches auger
    apo::XForm x2;
    double customFreq = 13.0;
    x2.setVariable("auger_freq", customFreq);

    apo::XForm out;
    out.interpolateVariablesFrom(x1, x2, 0.5, 0.5);

    double blended = -1;
    check(out.getVariable("auger_freq", blended) && approxEqual(blended, 0.5 * 5.0 + 0.5 * 13.0),
          "interpolateVariablesFrom blends x2's customized value against x1's true (unmaterialized) factory default");
}

void testInterpolateVariablesFromLeavesBothUntouchedSideAtDefault() {
    apo::XForm x1;
    apo::XForm x2;
    apo::XForm out;
    // c0+c1==1, matching MutationOps::blendXforms's only real call shape.
    out.interpolateVariablesFrom(x1, x2, 0.4, 0.6);

    double value = -1;
    check(out.getVariable("auger_freq", value) && approxEqual(value, 5.0),
          "interpolating two untouched sides (c0+c1==1) leaves the result at the factory default");
}

void testAssignCopiesNonDeterministicDefaultSourceExactly() {
    // pdj_a's default is drawn from constructionRandom01() at construction
    // (VarPDJ::VarPDJ()), not a fixed constant - the exact case
    // hasNonDeterministicConstructionDefault exists for (Variation.h).
    // assign() must reproduce src's own frozen value on dst, not let dst's
    // first real touch draw an independent random default of its own.
    apo::XForm src;
    apo::XForm dst;

    double srcValue = -1;
    check(src.getVariable("pdj_a", srcValue), "src's pdj_a is readable before assign()");

    dst.assign(src); // src never explicitly set pdj_a - it's still at its (random, frozen) construction default

    double dstValue = -1;
    check(dst.getVariable("pdj_a", dstValue) && approxEqual(dstValue, srcValue),
          "assign() reproduces a non-deterministic-default source's own frozen value exactly, not a fresh draw");
}

void testInterpolateVariablesFromMaterializesBothNonDeterministicSides() {
    apo::XForm x1; // never touches pdj_a - has its own random construction default
    apo::XForm x2; // ditto, a DIFFERENT random construction default (independent instance)

    double v1 = -1, v2 = -1;
    x1.getVariable("pdj_a", v1);
    x2.getVariable("pdj_a", v2);

    apo::XForm out;
    out.interpolateVariablesFrom(x1, x2, 0.5, 0.5);

    double blended = -1;
    check(out.getVariable("pdj_a", blended) && approxEqual(blended, 0.5 * v1 + 0.5 * v2),
          "interpolateVariablesFrom blends the two sides' own real (materialized) construction defaults, "
          "not two independent fresh draws");
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
    testGetVariableReadsFactoryDefaultBeforeAnyTouch();
    testResetVariableOnUntouchedVariationReturnsTrueWithoutChangingDefault();
    testAssignResetsUntouchedVariationBackToDefault();
    testAssignCopiesACustomizedValueFromSource();
    testInterpolateVariablesFromBlendsAgainstTrueDefaultWhenOnlyOneSideTouched();
    testInterpolateVariablesFromLeavesBothUntouchedSideAtDefault();
    testAssignCopiesNonDeterministicDefaultSourceExactly();
    testInterpolateVariablesFromMaterializesBothNonDeterministicSides();
    testJulianSpecializedDispatch();

    return apo_test::reportAndExit();
}
