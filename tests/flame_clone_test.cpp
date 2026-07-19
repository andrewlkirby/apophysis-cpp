// Tests for Flame::clone()/copyFrom()/cloneForRender() (src/core/Flame.h).
// cloneForRender() is FOLLOWUP_PLAN.txt's B1a: a Renderer-only lean clone
// that only instantiates xform[0..activeXForms-1] plus finalXform, instead
// of clone()'s full kMaxXForms+1 slots - these tests exist to pin down its
// narrower contract (which slots are real, which stay null, that the real
// ones are genuine deep copies) since nothing else in the codebase
// previously needed to distinguish "a real Flame" from "a real Flame's
// first N xform slots".

#include <memory>

#include "TestHelpers.h"
#include "core/Flame.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::unique_ptr<apo::Flame> makeFlameWithDistinctXForms(int activeCount) {
    auto flame = std::make_unique<apo::Flame>();
    flame->width = 321;
    flame->height = 234;
    flame->gamma = 3.5;
    flame->brightness = 2.25;
    flame->name = "clone-test-flame";
    for (int i = 0; i < activeCount; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0 + i; // distinct, and nonzero (numXForms() packing contract)
        xf.color = i / 10.0;
        xf.c[0] = {0.1 * i, 0.0};
        xf.c[2] = {i + 0.5, -i - 0.5};
    }
    flame->finalXformEnabled = true;
    flame->useFinalXform = true;
    flame->finalXform->clear();
    flame->finalXform->color = 0.75;
    flame->finalXform->c[2] = {1.5, -1.5};
    return flame;
}

void testCloneForRenderPopulatesOnlyTheRequestedActiveSlots() {
    auto flame = makeFlameWithDistinctXForms(3);
    auto rendered = flame->cloneForRender(3);

    check(rendered->xform[0] != nullptr, "cloneForRender(3) populates xform[0]");
    check(rendered->xform[1] != nullptr, "cloneForRender(3) populates xform[1]");
    check(rendered->xform[2] != nullptr, "cloneForRender(3) populates xform[2]");
    check(rendered->xform[3] == nullptr, "cloneForRender(3) leaves xform[3] (first unused slot) null");
    check(rendered->xform[apo::Flame::kMaxXForms - 1] == nullptr,
          "cloneForRender(3) leaves the last xform slot null too, not just the first unused one");
    check(rendered->finalXform != nullptr, "cloneForRender always populates finalXform regardless of activeXForms");
}

void testCloneForRenderDeepCopiesTheActiveXFormsAndFinalXform() {
    auto flame = makeFlameWithDistinctXForms(4);
    auto rendered = flame->cloneForRender(4);

    for (int i = 0; i < 4; ++i) {
        check(approxEqual(rendered->xform[i]->density, flame->xform[i]->density),
              "cloneForRender copies each active xform's density correctly");
        check(approxEqual(rendered->xform[i]->color, flame->xform[i]->color),
              "cloneForRender copies each active xform's color correctly");
        check(approxEqual(rendered->xform[i]->c[2][0], flame->xform[i]->c[2][0]) &&
                  approxEqual(rendered->xform[i]->c[2][1], flame->xform[i]->c[2][1]),
              "cloneForRender copies each active xform's affine coefficients correctly");
    }
    check(approxEqual(rendered->finalXform->color, 0.75), "cloneForRender copies finalXform's fields correctly");

    // A real deep copy, not aliasing - mutating the source afterward must
    // not be visible through the clone (the same guarantee clone() already
    // gives, now checked for cloneForRender() too since it's a separate
    // code path).
    flame->xform[0]->density = 999.0;
    check(!approxEqual(rendered->xform[0]->density, 999.0),
          "cloneForRender's xform copies are independent of the source (no aliasing)");
}

void testCloneForRenderCopiesScalarFlameFields() {
    auto flame = makeFlameWithDistinctXForms(2);
    auto rendered = flame->cloneForRender(2);

    check(rendered->width == 321 && rendered->height == 234, "cloneForRender copies width/height");
    check(approxEqual(rendered->gamma, 3.5) && approxEqual(rendered->brightness, 2.25),
          "cloneForRender copies scalar render-parameter fields (gamma/brightness)");
    check(rendered->name == "clone-test-flame", "cloneForRender copies string fields (name)");
    check(rendered->finalXformEnabled && rendered->useFinalXform,
          "cloneForRender copies finalXformEnabled/useFinalXform");
}

void testCloneForRenderClampsActiveXFormsToValidRange() {
    auto flame = makeFlameWithDistinctXForms(2);

    auto zero = flame->cloneForRender(0);
    check(zero->xform[0] == nullptr, "cloneForRender(0) leaves every xform slot null");
    check(zero->finalXform != nullptr, "cloneForRender(0) still populates finalXform");

    auto tooMany = flame->cloneForRender(apo::Flame::kMaxXForms + 50);
    check(tooMany->xform[apo::Flame::kMaxXForms - 1] != nullptr,
          "an out-of-range activeXForms clamps down to kMaxXForms rather than overrunning the array");

    auto negative = flame->cloneForRender(-5);
    check(negative->xform[0] == nullptr, "a negative activeXForms clamps to 0, not a crash");
}

void testCloneStillPopulatesEveryXFormSlot() {
    // clone() (the general-purpose, non-Renderer-only path every other
    // Flame consumer relies on) must be entirely unaffected by
    // cloneForRender()'s existence - every one of the full kMaxXForms+1
    // slots is still a real object, same as before this change.
    auto flame = makeFlameWithDistinctXForms(2);
    auto full = flame->clone();
    check(full->xform[0] != nullptr && full->xform[apo::Flame::kMaxXForms - 1] != nullptr,
          "clone() still populates every xform slot, unlike cloneForRender()");
    check(full->numXForms() == 2, "clone() still round-trips numXForms() correctly");
}

} // namespace

int main() {
    testCloneForRenderPopulatesOnlyTheRequestedActiveSlots();
    testCloneForRenderDeepCopiesTheActiveXFormsAndFinalXform();
    testCloneForRenderCopiesScalarFlameFields();
    testCloneForRenderClampsActiveXFormsToValidRange();
    testCloneStillPopulatesEveryXFormSlot();
    return apo_test::reportAndExit();
}
