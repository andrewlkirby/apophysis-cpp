// Real-widget interaction tests for CurvesDialog (src/ui/CurvesDialog.h)
// and CurvesWidget (src/ui/CurvesWidget.h) - the underlying Bezier math
// (core/Bezier.h) and its render-pipeline integration are already covered
// independently in bezier_test.cpp/renderer_test.cpp; this file verifies
// the Qt wiring on top of it: channel switching moves which curve the
// weight sliders/drags target, dragging a control point on the graph
// reaches the shared Flame, weight-slider edits reach the active channel's
// curve, and Reset/Reset All restore the dialog's captured starting point.
// Mirrors mutate_dialog_interaction_test.cpp's approach.

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include "../TestHelpers.h"
#include "CurvesDialog.h"
#include "CurvesWidget.h"
#include "SliderSpin.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->width = 48;
    flame->height = 48;
    flame->pixelsPerUnit = 24;
    apo::XForm& xf = *flame->xform[0];
    xf.clear();
    xf.density = 1.0;
    xf.c[0] = {1, 0};
    xf.c[1] = {0, 1};
    xf.setVariation(0, 1.0); // linear
    for (int i = 0; i < 256; ++i) flame->cmap.entries[i] = {i, i, i, 255};
    return flame;
}

void testControlsExposed() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::CurvesDialog(flame);

    auto* channelCombo = dialog->findChild<QComboBox*>("channelCombo");
    auto* curvesWidget = dialog->findChild<apo::ui::CurvesWidget*>("curvesWidget");
    auto* weight1Ctrl = dialog->findChild<apo::ui::SliderSpin*>("weight1Ctrl");
    auto* weight2Ctrl = dialog->findChild<apo::ui::SliderSpin*>("weight2Ctrl");
    auto* resetButton = dialog->findChild<QPushButton*>("resetButton");
    auto* resetAllButton = dialog->findChild<QPushButton*>("resetAllButton");
    if (!check(channelCombo && curvesWidget && weight1Ctrl && weight2Ctrl && resetButton && resetAllButton,
               "CurvesDialog exposes channel combo/curves widget/weight sliders/reset buttons")) {
        delete dialog;
        return;
    }

    check(channelCombo->count() == 4, "channel combo has exactly 4 entries (All/Red/Green/Blue)");
    check(channelCombo->currentIndex() == 0, "channel combo starts on 'All' (the master channel)");
    check(curvesWidget->activeChannel() == 0, "curves widget starts with the master channel active");
    check(approxEqual(weight1Ctrl->value(), 1.0) && approxEqual(weight2Ctrl->value(), 1.0),
          "weight sliders start at the default identity curve's weight (1.0)");

    delete dialog;
}

void testChannelSwitchRetargetsWeightSlidersAndActiveChannel() {
    auto flame = makeTestFlame();
    flame->curves[2].weights = {1.0, 3.5, 2.5, 1.0}; // Green channel, pre-set weights

    auto* dialog = new apo::ui::CurvesDialog(flame);
    auto* channelCombo = dialog->findChild<QComboBox*>("channelCombo");
    auto* curvesWidget = dialog->findChild<apo::ui::CurvesWidget*>("curvesWidget");
    auto* weight1Ctrl = dialog->findChild<apo::ui::SliderSpin*>("weight1Ctrl");
    auto* weight2Ctrl = dialog->findChild<apo::ui::SliderSpin*>("weight2Ctrl");
    if (!check(channelCombo && curvesWidget && weight1Ctrl && weight2Ctrl, "controls found")) {
        delete dialog;
        return;
    }

    channelCombo->setCurrentIndex(2); // Green
    check(curvesWidget->activeChannel() == 2, "selecting 'Green' in the combo retargets the curves widget's active channel");
    check(approxEqual(weight1Ctrl->value(), 3.5) && approxEqual(weight2Ctrl->value(), 2.5),
          "switching channels refreshes the weight sliders to that channel's own weights");

    delete dialog;
}

void testDraggingControlPointReachesFlame() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::CurvesDialog(flame);
    auto* curvesWidget = dialog->findChild<apo::ui::CurvesWidget*>("curvesWidget");
    if (!check(curvesWidget != nullptr, "curves widget found")) {
        delete dialog;
        return;
    }
    // curvesWidget is layout-managed (QSplitter -> QVBoxLayout), so its real
    // size only exists once the dialog is actually shown - same reasoning
    // as triangle_canvas_interaction_test.cpp's EditorWindow-canvas drag
    // test, which reads geometry off the shown widget rather than assuming
    // a resize() call sticks against a layout that will just fight it.
    dialog->show();

    QSignalSpy spy(dialog, &apo::ui::CurvesDialog::flameChanged);

    // Master channel's P1 defaults to curve-space (0,0), which - per
    // CurvesWidget::curveToWidget's y-flipped mapping - sits at the
    // widget's bottom-left corner; drag it up toward the widget's center
    // (curve-space ~(0.5,0.5)) to give the master curve real shape.
    const QPoint startPx(2, curvesWidget->height() - 3);
    const QPoint targetPx(curvesWidget->width() / 2, curvesWidget->height() / 2);

    QTest::mousePress(curvesWidget, Qt::LeftButton, Qt::NoModifier, startPx);
    QTest::mouseMove(curvesWidget, targetPx);
    QTest::mouseRelease(curvesWidget, Qt::LeftButton, Qt::NoModifier, targetPx);

    const auto& p1 = flame->curves[0].points[1];
    check(p1[0] > 0.3 && p1[0] < 0.7 && p1[1] > 0.3 && p1[1] < 0.7,
          "dragging the master channel's P1 handle toward the widget's center moves Flame::curves[0]'s P1 to "
          "roughly curve-space (0.5, 0.5)");
    check(spy.count() >= 1, "flameChanged fires at least once while dragging a control point");

    delete dialog;
}

void testWeightSliderReachesActiveChannelOnly() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::CurvesDialog(flame);
    auto* channelCombo = dialog->findChild<QComboBox*>("channelCombo");
    auto* weight1Ctrl = dialog->findChild<apo::ui::SliderSpin*>("weight1Ctrl");
    if (!check(channelCombo && weight1Ctrl, "channel combo and weight1Ctrl found")) {
        delete dialog;
        return;
    }

    channelCombo->setCurrentIndex(1); // Red
    auto* spin = weight1Ctrl->findChild<QDoubleSpinBox*>();
    spin->setValue(6.5);

    check(approxEqual(flame->curves[1].weights[1], 6.5), "editing Weight 1 while Red is active updates curves[1]'s weight");
    check(approxEqual(flame->curves[0].weights[1], 1.0) && approxEqual(flame->curves[2].weights[1], 1.0) &&
              approxEqual(flame->curves[3].weights[1], 1.0),
          "editing one channel's weight leaves every other channel's weight untouched");

    delete dialog;
}

void testResetChannelOnlyRestoresActiveChannel() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::CurvesDialog(flame);
    auto* channelCombo = dialog->findChild<QComboBox*>("channelCombo");
    auto* resetButton = dialog->findChild<QPushButton*>("resetButton");
    auto* weight1Ctrl = dialog->findChild<apo::ui::SliderSpin*>("weight1Ctrl");
    if (!check(channelCombo && resetButton && weight1Ctrl, "controls found")) {
        delete dialog;
        return;
    }

    channelCombo->setCurrentIndex(0); // All
    weight1Ctrl->findChild<QDoubleSpinBox*>()->setValue(9.0);
    channelCombo->setCurrentIndex(1); // Red
    weight1Ctrl->findChild<QDoubleSpinBox*>()->setValue(4.0);

    check(approxEqual(flame->curves[0].weights[1], 9.0), "master channel edit landed before switching away");

    channelCombo->setCurrentIndex(1); // still Red, the active channel
    QTest::mouseClick(resetButton, Qt::LeftButton);

    check(approxEqual(flame->curves[1].weights[1], 1.0), "Reset Channel restores only the active (Red) channel to its starting curve");
    check(approxEqual(flame->curves[0].weights[1], 9.0), "Reset Channel leaves other channels (master) untouched");

    delete dialog;
}

void testResetAllRestoresEveryChannel() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::CurvesDialog(flame);
    auto* channelCombo = dialog->findChild<QComboBox*>("channelCombo");
    auto* resetAllButton = dialog->findChild<QPushButton*>("resetAllButton");
    auto* weight1Ctrl = dialog->findChild<apo::ui::SliderSpin*>("weight1Ctrl");
    if (!check(channelCombo && resetAllButton && weight1Ctrl, "controls found")) {
        delete dialog;
        return;
    }

    channelCombo->setCurrentIndex(0);
    weight1Ctrl->findChild<QDoubleSpinBox*>()->setValue(7.0);
    channelCombo->setCurrentIndex(3);
    weight1Ctrl->findChild<QDoubleSpinBox*>()->setValue(8.0);

    QTest::mouseClick(resetAllButton, Qt::LeftButton);

    check(approxEqual(flame->curves[0].weights[1], 1.0) && approxEqual(flame->curves[3].weights[1], 1.0),
          "Reset All restores every channel's weight back to the dialog's captured starting curves");

    delete dialog;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testControlsExposed();
    testChannelSwitchRetargetsWeightSlidersAndActiveChannel();
    testDraggingControlPointReachesFlame();
    testWeightSliderReachesActiveChannelOnly();
    testResetChannelOnlyRestoresActiveChannel();
    testResetAllRestoresEveryChannel();

    return apo_test::reportAndExit();
}
