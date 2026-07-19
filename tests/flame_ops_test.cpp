// Tests for A6's weight/color flame-menu utility commands (src/core/edit/
// FlameOps.h) - kept Qt-free and unit-tested independently, mirroring
// gradient_ops_test.cpp's approach. Each formula is verified directly
// against Main.pas/ControlPoint.pas (see FlameOps.h's own doc comment) -
// "Compute Weights" is deliberately not covered here since it's dead code
// in the original itself (also documented there).

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/Rng.h"
#include "core/edit/FlameOps.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeFlameWithXForms(int n) {
    auto flame = std::make_shared<apo::Flame>();
    for (int i = 0; i < n; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.color = 0.0;
    }
    // Density=0 marks "inactive" (Flame::numXForms()'s own contract) - the
    // constructor already leaves every unused xform this way, but be
    // explicit for xforms beyond the requested count in case a future
    // Flame() default ever changes.
    for (int i = n; i < apo::Flame::kMaxXForms; ++i) flame->xform[i]->density = 0.0;
    return flame;
}

void testRandomizeXformWeights() {
    auto flame = makeFlameWithXForms(4);
    for (int i = 0; i < 4; ++i) flame->xform[i]->density = 1.0; // distinguishable starting value

    apo::Xoshiro256Rng rng(7);
    apo::randomizeXformWeights(*flame, rng);

    bool allInRange = true;
    bool anyDiffersFromOthers = false;
    for (int i = 0; i < 4; ++i) {
        const double d = flame->xform[i]->density;
        if (!(d >= 0.0 && d < 1.0)) allInRange = false;
        if (i > 0 && !approxEqual(d, flame->xform[0]->density)) anyDiffersFromOthers = true;
    }
    check(allInRange, "randomizeXformWeights sets every active xform's density into [0, 1)");
    check(anyDiffersFromOthers, "randomizeXformWeights draws an independent value per xform, not one shared value");
    check(approxEqual(flame->xform[4]->density, 0.0), "randomizeXformWeights leaves inactive xforms (beyond numXForms()) untouched");
}

void testEqualizeXformWeights() {
    auto flame = makeFlameWithXForms(3);
    flame->xform[0]->density = 0.1;
    flame->xform[1]->density = 0.9;
    flame->xform[2]->density = 0.4;

    apo::equalizeXformWeights(*flame);

    check(approxEqual(flame->xform[0]->density, 0.5) && approxEqual(flame->xform[1]->density, 0.5) &&
              approxEqual(flame->xform[2]->density, 0.5),
          "equalizeXformWeights sets every active xform's density to exactly 0.5, matching "
          "ControlPoint.pas's own literal constant (not 1/numXForms)");
}

void testCalculateXformColorValues() {
    auto flame = makeFlameWithXForms(3);
    flame->xform[0]->color = 0.9;
    flame->xform[1]->color = 0.9;
    flame->xform[2]->color = 0.9;

    apo::calculateXformColorValues(*flame);

    check(approxEqual(flame->xform[0]->color, 0.0), "calculateXformColorValues: first xform gets color 0");
    check(approxEqual(flame->xform[1]->color, 0.5), "calculateXformColorValues: middle xform gets color 0.5 (1/2)");
    check(approxEqual(flame->xform[2]->color, 1.0), "calculateXformColorValues: last xform gets color 1");
}

void testCalculateXformColorValuesSingleXformDoesNotDivideByZero() {
    auto flame = makeFlameWithXForms(1);
    flame->xform[0]->color = 0.42;

    apo::calculateXformColorValues(*flame);

    check(approxEqual(flame->xform[0]->color, 0.42),
          "calculateXformColorValues leaves a single xform's color untouched instead of dividing by "
          "(numXForms-1)=0, fixing the original's own real division-by-zero there");
}

void testRandomizeXformColorValues() {
    auto flame = makeFlameWithXForms(4);
    // Distinguishable starting value on every xform, active or not - index
    // 4 (inactive, beyond numXForms()==4) included, so the "left untouched"
    // check below compares against a known value rather than an
    // unspecified XForm class default.
    for (int i = 0; i <= 4; ++i) flame->xform[i]->color = 0.5;

    apo::Xoshiro256Rng rng(11);
    apo::randomizeXformColorValues(*flame, rng);

    bool allInRange = true;
    bool anyDiffersFromOthers = false;
    for (int i = 0; i < 4; ++i) {
        const double c = flame->xform[i]->color;
        if (!(c >= 0.0 && c < 1.0)) allInRange = false;
        if (i > 0 && !approxEqual(c, flame->xform[0]->color)) anyDiffersFromOthers = true;
    }
    check(allInRange, "randomizeXformColorValues sets every active xform's color into [0, 1)");
    check(anyDiffersFromOthers, "randomizeXformColorValues draws an independent value per xform, not one shared value");
    check(approxEqual(flame->xform[4]->color, 0.5),
          "randomizeXformColorValues leaves inactive xforms (beyond numXForms()) untouched");
}

} // namespace

int main() {
    testRandomizeXformWeights();
    testEqualizeXformWeights();
    testCalculateXformColorValues();
    testCalculateXformColorValuesSingleXformDoesNotDivideByZero();
    testRandomizeXformColorValues();
    return apo_test::reportAndExit();
}
