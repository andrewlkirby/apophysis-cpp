// Real-widget interaction tests for ForceSymmetryDialog
// (src/ui/ForceSymmetryDialog.h) - the underlying math
// (core/edit/Symmetry.h's addSymmetry) is already covered independently in
// symmetry_test.cpp; this file verifies the Qt wiring on top of it: the
// Kind/Order controls collapse to the correct signed `sym` value on
// Apply, and this dialog never mutates the flame itself (see its class
// comment - EditorWindow owns the actual mutation/undo, tested in
// triangle_canvas_interaction_test.cpp's Force Symmetry integration test).

#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>

#include "../TestHelpers.h"
#include "ForceSymmetryDialog.h"

using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->xform[0]->density = 1.0;
    return flame;
}

void testControlsExposedWithSensibleDefaults() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::ForceSymmetryDialog(flame);

    auto* kindCombo = dialog->findChild<QComboBox*>("kindCombo");
    auto* orderSpin = dialog->findChild<QSpinBox*>("orderSpin");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    auto* closeButton = dialog->findChild<QPushButton*>("closeButton");
    if (!check(kindCombo && orderSpin && applyButton && closeButton,
               "ForceSymmetryDialog exposes kindCombo/orderSpin/applyButton/closeButton")) {
        delete dialog;
        return;
    }

    check(kindCombo->count() == 2, "kind combo has exactly 2 entries (Rotational / Rotational + Reflective)");
    check(kindCombo->currentIndex() == 0, "kind combo starts on Rotational");
    check(orderSpin->minimum() == 2, "order spin's minimum is 2 (order 1 is a documented no-op)");
    check(orderSpin->value() >= 2, "order spin starts at a usable (>=2) default");

    delete dialog;
}

void testApplyEmitsPositiveSymForRotationalKind() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::ForceSymmetryDialog(flame);
    auto* kindCombo = dialog->findChild<QComboBox*>("kindCombo");
    auto* orderSpin = dialog->findChild<QSpinBox*>("orderSpin");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(kindCombo && orderSpin && applyButton, "controls found")) {
        delete dialog;
        return;
    }

    kindCombo->setCurrentIndex(0); // Rotational
    orderSpin->setValue(6);

    QSignalSpy spy(dialog, &apo::ui::ForceSymmetryDialog::applyRequested);
    QTest::mouseClick(applyButton, Qt::LeftButton);

    check(spy.count() == 1, "clicking Apply emits applyRequested exactly once");
    if (spy.count() == 1) {
        check(spy.at(0).at(0).toInt() == 6, "Rotational kind at order 6 emits sym=+6");
    }

    delete dialog;
}

void testApplyEmitsNegativeSymForReflectiveKind() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::ForceSymmetryDialog(flame);
    auto* kindCombo = dialog->findChild<QComboBox*>("kindCombo");
    auto* orderSpin = dialog->findChild<QSpinBox*>("orderSpin");
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(kindCombo && orderSpin && applyButton, "controls found")) {
        delete dialog;
        return;
    }

    kindCombo->setCurrentIndex(1); // Rotational + Reflective
    orderSpin->setValue(5);

    QSignalSpy spy(dialog, &apo::ui::ForceSymmetryDialog::applyRequested);
    QTest::mouseClick(applyButton, Qt::LeftButton);

    check(spy.count() == 1, "clicking Apply emits applyRequested exactly once");
    if (spy.count() == 1) {
        check(spy.at(0).at(0).toInt() == -5, "Rotational + Reflective kind at order 5 emits sym=-5");
    }

    delete dialog;
}

void testApplyNeverMutatesFlameItself() {
    auto flame = makeTestFlame();
    const double originalC00 = flame->xform[0]->c[0][0];
    auto* dialog = new apo::ui::ForceSymmetryDialog(flame);
    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(applyButton != nullptr, "applyButton found")) {
        delete dialog;
        return;
    }

    QTest::mouseClick(applyButton, Qt::LeftButton);

    check(flame->numXForms() == 1 && flame->xform[0]->c[0][0] == originalC00,
          "ForceSymmetryDialog never mutates the flame itself - it only ever emits applyRequested(sym); the "
          "owning EditorWindow performs the actual addSymmetry() call and undo bookkeeping");

    delete dialog;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testControlsExposedWithSensibleDefaults();
    testApplyEmitsPositiveSymForRotationalKind();
    testApplyEmitsNegativeSymForReflectiveKind();
    testApplyNeverMutatesFlameItself();

    return apo_test::reportAndExit();
}
