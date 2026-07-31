// Real-widget interaction tests for AdjustPanel (src/ui/AdjustPanel.h) -
// the embeddable Camera/Coloring/Gradient/Size control widget extracted out
// of AdjustDialog (see AdjustDialog.h's own class comment) so EditorWindow
// can host it directly as a right-hand panel. AdjustDialog's own test suite
// (adjust_dialog_interaction_test.cpp) already covers the underlying math
// end-to-end through the dialog; this file instead verifies AdjustPanel's
// own host-facing contract in isolation - propertyEdited()/editingStarted()/
// editingFinished() fire exactly like TransformPanel's do (see
// transform_panel_interaction_test.cpp), with no host window required.

#include <memory>

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>

#include "../TestHelpers.h"
#include "AdjustPanel.h"
#include "SliderSpin.h"
#include "core/edit/GradientOps.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->width = 64;
    flame->height = 64;
    flame->zoom = 0;
    flame->gamma = 4.0;
    flame->pixelsPerUnit = 50;
    return flame;
}

void typeIntoSliderSpin(apo::ui::SliderSpin* ctrl, double value) {
    auto* spin = ctrl->findChild<QDoubleSpinBox*>();
    spin->setFocus();
    spin->setValue(value);
    QTest::keyClick(spin, Qt::Key_Return);
}

void testZoomEditUpdatesFlameAndFiresPropertyEdited() {
    auto flame = makeTestFlame();
    auto* panel = new apo::ui::AdjustPanel(flame);
    auto* zoomCtrl = panel->findChild<apo::ui::SliderSpin*>("zoomCtrl");
    if (!check(zoomCtrl != nullptr, "AdjustPanel exposes a zoomCtrl SliderSpin")) {
        delete panel;
        return;
    }

    QSignalSpy propertySpy(panel, &apo::ui::AdjustPanel::propertyEdited);
    typeIntoSliderSpin(zoomCtrl, 1.25);
    check(approxEqual(flame->zoom, 1.25), "typing into the Zoom spin box updates Flame::zoom");
    check(propertySpy.count() >= 1, "propertyEdited fires at least once for the edit");

    delete panel;
}

void testGestureBracketsExactlyOneEditingStartedAndFinished() {
    auto flame = makeTestFlame();
    auto* panel = new apo::ui::AdjustPanel(flame);
    auto* gammaCtrl = panel->findChild<apo::ui::SliderSpin*>("gammaCtrl");
    if (!check(gammaCtrl != nullptr, "AdjustPanel exposes a gammaCtrl SliderSpin")) {
        delete panel;
        return;
    }

    QSignalSpy startedSpy(panel, &apo::ui::AdjustPanel::editingStarted);
    QSignalSpy finishedSpy(panel, &apo::ui::AdjustPanel::editingFinished);
    typeIntoSliderSpin(gammaCtrl, 7.5);

    check(startedSpy.count() == 1, "editingStarted fires exactly once for a single committed edit");
    check(finishedSpy.count() == 1, "editingFinished fires exactly once for a single committed edit");

    delete panel;
}

void testGradientRotateModeMatchesGradientOps() {
    auto flame = makeTestFlame();
    for (int i = 0; i < 256; ++i) flame->cmap.entries[i] = {i, 255 - i, 128, 255};
    const apo::ColorMap baseline = flame->cmap;

    auto* panel = new apo::ui::AdjustPanel(flame);
    auto* modeCombo = panel->findChild<QComboBox*>("gradientModeCombo");
    auto* amountCtrl = panel->findChild<apo::ui::SliderSpin*>("gradientAmountCtrl");
    if (!check(modeCombo != nullptr && amountCtrl != nullptr, "AdjustPanel exposes gradient mode combo + amount control")) {
        delete panel;
        return;
    }

    typeIntoSliderSpin(amountCtrl, 20);
    const apo::ColorMap expected = apo::rotateGradient(baseline, 20);
    check(flame->cmap.entries == expected.entries,
          "the Gradient tab's Rotate mode produces exactly what GradientOps::rotateGradient computes");

    delete panel;
}

void testSizeTabApplyRescalesFlame() {
    auto flame = makeTestFlame();
    flame->width = 100;
    flame->height = 100;
    flame->pixelsPerUnit = 50;

    auto* panel = new apo::ui::AdjustPanel(flame);
    auto* widthSpin = panel->findChild<QSpinBox*>("widthSpin");
    auto* heightSpin = panel->findChild<QSpinBox*>("heightSpin");
    auto* applyButton = panel->findChild<QPushButton*>("sizeApplyButton");
    if (!check(widthSpin && heightSpin && applyButton, "Size tab controls found")) {
        delete panel;
        return;
    }

    QSignalSpy startedSpy(panel, &apo::ui::AdjustPanel::editingStarted);
    QSignalSpy finishedSpy(panel, &apo::ui::AdjustPanel::editingFinished);
    widthSpin->setValue(200);
    heightSpin->setValue(150);
    QTest::mouseClick(applyButton, Qt::LeftButton);

    check(flame->width == 200 && flame->height == 150, "Apply updates Flame::width/height");
    check(approxEqual(flame->pixelsPerUnit, 100.0), "Apply rescales pixelsPerUnit proportionally");
    check(startedSpy.count() == 1 && finishedSpy.count() == 1,
          "the Apply button click brackets exactly one editingStarted/editingFinished gesture");

    delete panel;
}

void testRefreshControlsFromFlameResyncsAfterExternalMutation() {
    auto flame = makeTestFlame();
    auto* panel = new apo::ui::AdjustPanel(flame);
    auto* zoomCtrl = panel->findChild<apo::ui::SliderSpin*>("zoomCtrl");
    if (!check(zoomCtrl != nullptr, "zoomCtrl found")) {
        delete panel;
        return;
    }

    // Mutate flame_ directly (as an undo/redo applying a snapshot would),
    // bypassing the panel entirely - refreshControlsFromFlame() is the
    // host's hook to catch the widget back up.
    flame->zoom = 2.5;
    panel->refreshControlsFromFlame();
    check(approxEqual(zoomCtrl->value(), 2.5), "refreshControlsFromFlame() re-reads Flame::zoom into zoomCtrl");

    delete panel;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testZoomEditUpdatesFlameAndFiresPropertyEdited();
    testGestureBracketsExactlyOneEditingStartedAndFinished();
    testGradientRotateModeMatchesGradientOps();
    testSizeTabApplyRescalesFlame();
    testRefreshControlsFromFlameResyncsAfterExternalMutation();

    return apo_test::reportAndExit();
}
