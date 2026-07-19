// Tests for core/edit/Symmetry.h's addSymmetry() - the Forced Symmetry
// math, ported from ControlPoint.pas's add_symmetry_to_control_point and
// verified directly against source (see Symmetry.h's doc comment for the
// sym-sign convention).

#include <cmath>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/Rng.h"
#include "core/edit/Symmetry.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

// Not <cmath>'s M_PI: MSVC only defines that behind _USE_MATH_DEFINES (see
// e.g. AdjustDialog.cpp's identical local kPi for the same reasoning).
constexpr double kPi = 3.14159265358979323846;

std::unique_ptr<apo::Flame> makeBaseFlame() {
    auto flame = std::make_unique<apo::Flame>();
    apo::XForm& xf = *flame->xform[0];
    xf.clear();
    xf.density = 1.0;
    return flame;
}

void testNoOpForZeroAndOne() {
    auto flameA = makeBaseFlame();
    check(apo::addSymmetry(*flameA, 0) == 0 && flameA->numXForms() == 1, "sym=0 is a no-op");

    auto flameB = makeBaseFlame();
    check(apo::addSymmetry(*flameB, 1) == 0 && flameB->numXForms() == 1, "sym=1 (order-1 rotation, i.e. identity) is a no-op");
}

void testPureRotationalSymmetryAppendsSymMinusOneXforms() {
    auto flame = makeBaseFlame();
    const int added = apo::addSymmetry(*flame, 4);

    check(added == 3, "sym=4 appends 3 rotation xforms (order 4, no reflection)");
    check(flame->numXForms() == 4, "the flame now has the original xform plus 3 rotations");
    check(flame->symmetry == 4, "Flame::symmetry records the applied order, matching cp.symmetry := sym");

    // k=1: angle = 2*pi/4 = pi/2 -> rotation matrix [[0,1],[-1,0]] in this
    // port's c[0]=(a,d), c[1]=(b,e) layout (c[0]=(cos,sin), c[1]=(-sin,cos)).
    const apo::XForm& r1 = *flame->xform[1];
    check(approxEqual(r1.c[0][0], std::cos(kPi / 2), 1e-9) && approxEqual(r1.c[0][1], std::sin(kPi / 2), 1e-9),
          "the first appended rotation xform's c[0] is (cos(2*pi/4), sin(2*pi/4))");
    check(approxEqual(r1.c[1][0], -r1.c[0][1], 1e-9) && approxEqual(r1.c[1][1], r1.c[0][0], 1e-9),
          "c[1] is the perpendicular partner (-sin, cos), completing a pure rotation matrix");
    check(approxEqual(r1.density, 1.0) && approxEqual(r1.variation(0), 1.0) && approxEqual(r1.symmetry, 1.0),
          "appended rotation xforms are pure-linear, density 1, and pinned (symmetry=1)");

    // k=3 (the last rotation, sym-1=3): angle = 3*pi/2.
    const apo::XForm& r3 = *flame->xform[3];
    check(approxEqual(r3.c[0][0], std::cos(3 * kPi / 2), 1e-9) && approxEqual(r3.c[0][1], std::sin(3 * kPi / 2), 1e-9),
          "the last appended rotation xform uses angle 2*pi*(sym-1)/sym");
}

void testNegativeOneAddsOnlyAReflection() {
    auto flame = makeBaseFlame();
    const int added = apo::addSymmetry(*flame, -1);

    check(added == 1, "sym=-1 appends exactly one xform (the reflection, no rotations)");
    check(flame->numXForms() == 2, "the flame now has the original xform plus one reflection");

    const apo::XForm& refl = *flame->xform[1];
    check(approxEqual(refl.c[0][0], -1.0) && approxEqual(refl.c[0][1], 0.0) && approxEqual(refl.c[1][0], 0.0) &&
              approxEqual(refl.c[1][1], 1.0),
          "the reflection xform is a horizontal flip: c[0]=(-1,0), c[1]=(0,1)");
    check(approxEqual(refl.color, 1.0), "the reflection xform's color is fixed at 1.0, matching the original");
}

void testNegativeSymAddsReflectionPlusRotations() {
    auto flame = makeBaseFlame();
    const int added = apo::addSymmetry(*flame, -3);

    check(added == 3, "sym=-3 appends 1 reflection + (3-1)=2 rotations = 3 xforms total");
    check(flame->numXForms() == 4, "the flame now has the original xform plus 3 symmetry xforms");

    const apo::XForm& refl = *flame->xform[1];
    check(approxEqual(refl.c[0][0], -1.0) && approxEqual(refl.c[0][1], 0.0),
          "xform 1 is still the reflection, appended before the rotations");

    const apo::XForm& r1 = *flame->xform[2];
    check(approxEqual(r1.c[0][0], std::cos(2 * kPi / 3), 1e-9) && approxEqual(r1.c[0][1], std::sin(2 * kPi / 3), 1e-9),
          "the rotations use the positive (de-negated) order - here 3, not -3");
}

void testAppendedXformsPreserveExistingOnes() {
    auto flame = makeBaseFlame();
    flame->xform[0]->color = 0.42;
    flame->xform[0]->c[2] = {1.5, -2.5}; // a distinctive translation

    apo::addSymmetry(*flame, 3);

    check(approxEqual(flame->xform[0]->color, 0.42) && flame->xform[0]->c[2][0] == 1.5 && flame->xform[0]->c[2][1] == -2.5,
          "addSymmetry only appends new xforms - it never touches the flame's pre-existing ones");
}

void testStopsAtMaxXFormsWithoutOverflow() {
    auto flame = makeBaseFlame();
    // Fill every xform slot but the last one, so only 1 free slot remains.
    for (int i = 1; i < apo::Flame::kMaxXForms - 1; ++i) {
        flame->xform[i]->density = 1.0;
    }
    check(flame->numXForms() == apo::Flame::kMaxXForms - 1, "the flame is set up with exactly one free xform slot");

    const int added = apo::addSymmetry(*flame, 10); // would normally add 9 rotations
    check(added == 1, "addSymmetry stops at the array's actual capacity instead of overflowing kMaxXForms");
    check(flame->numXForms() == apo::Flame::kMaxXForms, "the flame ends up exactly at kMaxXForms, not beyond it");

    auto fullFlame = makeBaseFlame();
    for (int i = 1; i < apo::Flame::kMaxXForms; ++i) fullFlame->xform[i]->density = 1.0;
    check(apo::addSymmetry(*fullFlame, 5) == 0, "addSymmetry on an already-full flame is a no-op returning 0");
}

} // namespace

int main() {
    testNoOpForZeroAndOne();
    testPureRotationalSymmetryAppendsSymMinusOneXforms();
    testNegativeOneAddsOnlyAReflection();
    testNegativeSymAddsReflectionPlusRotations();
    testAppendedXformsPreserveExistingOnes();
    testStopsAtMaxXFormsWithoutOverflow();

    return apo_test::reportAndExit();
}
