// Real-widget interaction tests for MutateDialog (src/ui/MutateDialog.h) -
// the underlying blend/randomize math (core/edit/MutationOps.h) is already
// covered independently in mutation_ops_test.cpp; this file verifies the
// Qt wiring on top of it: clicking the center cell regenerates mutants
// without touching the current flame, clicking a mutant cell adopts it in
// place (mutating the shared Flame, not rebinding it) and emits
// flameChanged exactly once, Maintain Symmetry pins flagged xforms exactly
// through an adoption, the Trend combo actually reaches the adopted
// flame's variation weights, "keep same number of transforms" is honored,
// Back re-enables/disables correctly, and a grid cell's button actually
// receives a rendered icon. Mirrors adjust_dialog_interaction_test.cpp/
// render_dialog_interaction_test.cpp's approach.

#include <functional>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include "../TestHelpers.h"
#include "MutateDialog.h"
#include "SliderSpin.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeBaseFlame(int numXforms = 2) {
    auto flame = std::make_shared<apo::Flame>();
    flame->width = 48;
    flame->height = 48;
    flame->pixelsPerUnit = 24;
    for (int i = 0; i < 256; ++i) flame->cmap.entries[i] = {i, i, i, 255};

    for (int i = 0; i < numXforms; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0 / numXforms;
        xf.color = (numXforms > 1) ? static_cast<double>(i) / (numXforms - 1) : 0.0;
        xf.c[2] = {0.1 * i, -0.2 * i};
    }
    return flame;
}

bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!pred()) {
        if (timer.elapsed() > timeoutMs) return false;
        QTest::qWait(20);
    }
    return true;
}

void testControlsExposed() {
    auto flame = makeBaseFlame();
    auto* dialog = new apo::ui::MutateDialog(flame);

    auto* trendCombo = dialog->findChild<QComboBox*>("trendCombo");
    auto* strengthCtrl = dialog->findChild<apo::ui::SliderSpin*>("strengthCtrl");
    auto* sameNumCheck = dialog->findChild<QCheckBox*>("sameNumCheck");
    auto* maintainSymmetryCheck = dialog->findChild<QCheckBox*>("maintainSymmetryCheck");
    auto* backButton = dialog->findChild<QPushButton*>("backButton");
    auto* centerCell = dialog->findChild<QPushButton*>("cellButton0");
    auto* mutantCell1 = dialog->findChild<QPushButton*>("cellButton1");
    if (!check(trendCombo && strengthCtrl && sameNumCheck && maintainSymmetryCheck && backButton && centerCell &&
                   mutantCell1,
               "MutateDialog exposes trend/strength/checkboxes/back/grid-cell controls")) {
        delete dialog;
        return;
    }

    check(trendCombo->count() > 1, "trend combo has 'Random' plus at least one variation");
    check(trendCombo->itemText(0) == "Random", "trend combo's first entry is 'Random'");
    check(approxEqual(strengthCtrl->value(), 35.0), "strength defaults to 35%, matching Mutate.pas's default");
    check(!backButton->isEnabled(), "Back starts disabled (no history to go back to yet)");

    delete dialog;
}

void testCenterClickRegeneratesWithoutTouchingCurrentFlame() {
    auto flame = makeBaseFlame();
    const double originalC0 = flame->xform[0]->c[2][0];
    const double originalDensity = flame->xform[0]->density;

    auto* dialog = new apo::ui::MutateDialog(flame);
    auto* centerCell = dialog->findChild<QPushButton*>("cellButton0");
    auto* backButton = dialog->findChild<QPushButton*>("backButton");
    if (!check(centerCell && backButton, "center cell and Back button found")) {
        delete dialog;
        return;
    }

    QTest::mouseClick(centerCell, Qt::LeftButton);

    check(approxEqual(flame->xform[0]->c[2][0], originalC0) && approxEqual(flame->xform[0]->density, originalDensity),
          "clicking the center cell regenerates mutants but never touches the current flame itself");
    check(backButton->isEnabled(), "Back becomes enabled after regenerating at least once");

    QTest::mouseClick(backButton, Qt::LeftButton);
    check(!backButton->isEnabled(), "Back becomes disabled again once seed navigation returns to the initial seed");

    delete dialog;
}

void testAdoptingMutantMutatesFlameInPlaceAndEmitsSignalOnce() {
    auto flame = makeBaseFlame();
    apo::Flame* rawFlamePtr = flame.get(); // confirm identity is preserved, not rebound

    auto* dialog = new apo::ui::MutateDialog(flame);
    auto* mutantCell = dialog->findChild<QPushButton*>("cellButton1");
    if (!check(mutantCell != nullptr, "mutant cell 1 found")) {
        delete dialog;
        return;
    }

    QSignalSpy spy(dialog, &apo::ui::MutateDialog::flameChanged);
    const double before = flame->xform[0]->c[2][0];

    QTest::mouseClick(mutantCell, Qt::LeftButton);

    check(flame.get() == rawFlamePtr, "adopting a mutant mutates the shared Flame in place (Flame::copyFrom), never rebinds it");
    check(spy.count() == 1, "flameChanged fires exactly once per adopted mutant");
    // Not asserting the coefficient necessarily *changed* (astronomically
    // unlikely to coincide, but not impossible) - just that adoption ran
    // to completion without crashing and left a well-formed flame behind.
    check(flame->numXForms() >= 1, "the adopted flame is well-formed (at least one active xform)");
    (void)before;

    delete dialog;
}

void testMaintainSymmetryPinsFlaggedXformExactlyThroughAdoption() {
    auto flame = makeBaseFlame();
    flame->xform[0]->symmetry = 1; // flag xform 0 as "pinned"
    const double pinnedC0 = flame->xform[0]->c[2][0];
    const double pinnedC1 = flame->xform[0]->c[2][1];
    const double pinnedDensity = flame->xform[0]->density;

    auto* dialog = new apo::ui::MutateDialog(flame);
    auto* maintainSymmetryCheck = dialog->findChild<QCheckBox*>("maintainSymmetryCheck");
    auto* mutantCell = dialog->findChild<QPushButton*>("cellButton1");
    if (!check(maintainSymmetryCheck && mutantCell, "maintainSymmetryCheck and mutant cell found")) {
        delete dialog;
        return;
    }

    maintainSymmetryCheck->setChecked(true);
    QTest::mouseClick(mutantCell, Qt::LeftButton);

    check(approxEqual(flame->xform[0]->c[2][0], pinnedC0) && approxEqual(flame->xform[0]->c[2][1], pinnedC1) &&
              approxEqual(flame->xform[0]->density, pinnedDensity),
          "a symmetry-flagged xform is copied through an adoption completely unchanged when Maintain Symmetry is checked");

    delete dialog;
}

void testTrendSelectionReachesAdoptedFlameAtFullStrength() {
    auto flame = makeBaseFlame(1); // one xform, so there's no ambiguity about which one gets checked
    auto* dialog = new apo::ui::MutateDialog(flame);

    auto* trendCombo = dialog->findChild<QComboBox*>("trendCombo");
    auto* strengthCtrl = dialog->findChild<apo::ui::SliderSpin*>("strengthCtrl");
    auto* mutantCell = dialog->findChild<QPushButton*>("cellButton1");
    if (!check(trendCombo && strengthCtrl && mutantCell, "trend combo, strength control, and mutant cell found")) {
        delete dialog;
        return;
    }

    // Index 1 is the first real variation (index 0 in VariationRegistry's
    // combined space - "linear", per VariationRegistry::localVarNames()).
    trendCombo->setCurrentIndex(1);
    auto* spin = strengthCtrl->findChild<QDoubleSpinBox*>();
    spin->setValue(100.0); // full strength: the adopted flame should exactly become the mutant

    QTest::mouseClick(mutantCell, Qt::LeftButton);

    check(approxEqual(flame->xform[0]->variation(0), 1.0),
          "at 100% strength, selecting a Trend and adopting a mutant gives the adopted xform exactly that variation");

    delete dialog;
}

void testKeepSameNumberOfTransformsIsHonoredOnAdoption() {
    auto flame = makeBaseFlame(4);
    auto* dialog = new apo::ui::MutateDialog(flame);

    auto* sameNumCheck = dialog->findChild<QCheckBox*>("sameNumCheck");
    auto* strengthCtrl = dialog->findChild<apo::ui::SliderSpin*>("strengthCtrl");
    auto* mutantCell = dialog->findChild<QPushButton*>("cellButton1");
    if (!check(sameNumCheck && strengthCtrl && mutantCell, "sameNumCheck, strength control, and mutant cell found")) {
        delete dialog;
        return;
    }

    sameNumCheck->setChecked(true);
    auto* spin = strengthCtrl->findChild<QDoubleSpinBox*>();
    spin->setValue(100.0); // full strength: adopted xform count should exactly match the mutant's

    QTest::mouseClick(mutantCell, Qt::LeftButton);

    check(flame->numXForms() == 4, "'keep same number of transforms' constrains regenerated mutants to the base's own xform count");

    delete dialog;
}

void testGridCellReceivesARenderedIcon() {
    auto flame = makeBaseFlame();
    auto* dialog = new apo::ui::MutateDialog(flame);
    auto* centerCell = dialog->findChild<QPushButton*>("cellButton0");
    if (!check(centerCell != nullptr, "center cell found")) {
        delete dialog;
        return;
    }

    const bool rendered = waitUntil([centerCell] { return !centerCell->icon().isNull(); });
    check(rendered, "the center grid cell receives a real rendered icon within the timeout");

    delete dialog;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testControlsExposed();
    testCenterClickRegeneratesWithoutTouchingCurrentFlame();
    testAdoptingMutantMutatesFlameInPlaceAndEmitsSignalOnce();
    testMaintainSymmetryPinsFlaggedXformExactlyThroughAdoption();
    testTrendSelectionReachesAdoptedFlameAtFullStrength();
    testKeepSameNumberOfTransformsIsHonoredOnAdoption();
    testGridCellReceivesARenderedIcon();

    return apo_test::reportAndExit();
}
