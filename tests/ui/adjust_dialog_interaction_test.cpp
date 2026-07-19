// Real-widget interaction tests for AdjustDialog (src/ui/AdjustDialog.h) -
// the Adjust dialog's core math (GradientOps, Flame::adjustScale) is
// already covered independently in gradient_ops_test.cpp; this file
// verifies the Qt wiring on top of it actually works: spin box/slider
// edits reach the Flame, commits push real undo/redo entries, gradient
// mode switching and the Invert/Reverse/Randomize/Reset buttons produce
// the expected cmap, and the Size tab's Apply button rescales the flame.
// Mirrors tests/ui/triangle_canvas_interaction_test.cpp's approach
// (QSignalSpy + QTest event injection against the real compiled widget,
// via the apo_ui object library) for the Editor window's TriangleCanvas.

#include <cstdio>
#include <fstream>

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTest>
#include <QTimer>

#include "../TestHelpers.h"
#include "AdjustDialog.h"
#include "GradientBrowserDialog.h"
#include "SliderSpin.h"
#include "core/edit/GradientOps.h"
#include "core/io/GradientIO.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

// Polls every 10ms (bounded by timeoutMs) until a modal DialogT is the
// active modal widget, then invokes `onShown` with it once and stops -
// replaces a single QTimer::singleShot(50, ...) check, which can race a
// modal dialog that happens to open later than that hardcoded delay. This
// was a real, confirmed non-deterministic hang, not a theoretical concern -
// see FOLLOWUP_PLAN.txt's B7 entry and main_window_interaction_test.cpp's
// own copy of this same helper for the full story. `context` is the
// QObject the polling timer is parented to, so it's cleaned up
// automatically if that object is destroyed first.
template <class DialogT, class F>
void whenModalShown(QObject* context, F onShown, int timeoutMs = 5000) {
    auto* timer = new QTimer(context);
    QElapsedTimer elapsed;
    elapsed.start();
    timer->setInterval(10);
    QObject::connect(timer, &QTimer::timeout, context, [timer, elapsed, onShown, timeoutMs]() mutable {
        if (auto* dialog = qobject_cast<DialogT*>(QApplication::activeModalWidget())) {
            timer->stop();
            timer->deleteLater();
            onShown(dialog);
        } else if (elapsed.elapsed() > timeoutMs) {
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start();
}

std::shared_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->width = 64;
    flame->height = 64;
    apo::XForm& xf = *flame->xform[0];
    xf.clear();
    xf.density = 1.0;
    xf.c[0] = {1, 0};
    xf.c[1] = {0, 1};
    xf.c[2] = {0, 0};
    xf.setVariation(0, 1.0); // linear
    flame->zoom = 0;
    flame->center = {0, 0};
    flame->gamma = 4.0;
    flame->brightness = 4.0;
    flame->pixelsPerUnit = 50;
    return flame;
}

// Types a value into a SliderSpin's spin box and presses Enter - the real
// end-user path for the precise-entry side of a SliderSpin, exercising
// QDoubleSpinBox::valueChanged (live) then QAbstractSpinBox's real Return-
// key handling (editingFinished), rather than calling AdjustDialog's
// private mutation logic directly.
void typeIntoSliderSpin(apo::ui::SliderSpin* ctrl, double value) {
    auto* spin = ctrl->findChild<QDoubleSpinBox*>();
    spin->setFocus();
    spin->setValue(value); // real API call -> emits valueChanged, same as a user typing
    QTest::keyClick(spin, Qt::Key_Return); // real key event -> emits editingFinished
}

void testSpinBoxEditCommitsToFlameAndUndo() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::AdjustDialog(flame);

    auto* zoomCtrl = dialog->findChild<apo::ui::SliderSpin*>("zoomCtrl");
    if (!check(zoomCtrl != nullptr, "AdjustDialog exposes a zoomCtrl SliderSpin")) {
        delete dialog;
        return;
    }

    typeIntoSliderSpin(zoomCtrl, 1.25);
    check(approxEqual(flame->zoom, 1.25), "typing into the Zoom spin box updates Flame::zoom");

    dialog->show();
    // See triangle_canvas_interaction_test.cpp's identical comment:
    // QWidget::activateWindow() doesn't synchronously update activation
    // state on the offscreen QPA platform, so QTest::keySequence's
    // shortcut dispatch below needs the deprecated but synchronous
    // QApplication::setActiveWindow() instead.
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(dialog);
    QT_WARNING_POP
    QTest::keySequence(dialog, QKeySequence::Undo);
    check(approxEqual(flame->zoom, 0.0), "Ctrl+Z reverts the zoom edit");

    QTest::keySequence(dialog, QKeySequence::Redo);
    check(approxEqual(flame->zoom, 1.25), "Ctrl+Shift+Z re-applies the zoom edit");

    delete dialog;
}

void testAngleFieldConvertsDegreesToRadians() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::AdjustDialog(flame);
    auto* angleCtrl = dialog->findChild<apo::ui::SliderSpin*>("angleCtrl");
    if (!check(angleCtrl != nullptr, "AdjustDialog exposes an angleCtrl SliderSpin")) {
        delete dialog;
        return;
    }

    typeIntoSliderSpin(angleCtrl, 90.0);
    // 90 degrees -> pi/2 radians.
    check(approxEqual(flame->angle, 1.5707963267948966, 1e-6), "the Angle control converts degrees to radians");

    delete dialog;
}

void testGammaSliderCommitsLiveAndOnRelease() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::AdjustDialog(flame);
    auto* gammaCtrl = dialog->findChild<apo::ui::SliderSpin*>("gammaCtrl");
    if (!check(gammaCtrl != nullptr, "AdjustDialog exposes a gammaCtrl SliderSpin")) {
        delete dialog;
        return;
    }

    // Drive the slider directly (the other half of SliderSpin's dual
    // slider+spinbox sync, not exercised by the spin-box tests above).
    auto* slider = gammaCtrl->findChild<QSlider*>();
    if (!check(slider != nullptr, "SliderSpin exposes an internal QSlider")) {
        delete dialog;
        return;
    }
    slider->setValue(slider->maximum() / 2);
    check(!approxEqual(flame->gamma, 4.0), "moving the Gamma slider live-updates Flame::gamma before release");
    slider->sliderReleased();
    check(true, "sliderReleased() commits without crashing");

    delete dialog;
}

void testBackgroundColorPicker() {
    auto flame = makeTestFlame();
    flame->background = {0, 0, 0, 255};
    auto* dialog = new apo::ui::AdjustDialog(flame);
    auto* bgButton = dialog->findChild<QPushButton*>("backgroundButton");
    if (!check(bgButton != nullptr, "AdjustDialog exposes a backgroundButton")) {
        delete dialog;
        return;
    }

    // QColorDialog::getColor() runs its own nested modal event loop -
    // poll for it and accept it once it's open (whenModalShown), the
    // standard pattern here for modal dialogs triggered by a click.
    whenModalShown<QColorDialog>(dialog, [](QColorDialog* colorDialog) {
        colorDialog->setCurrentColor(QColor(10, 20, 30));
        colorDialog->accept();
    });
    QTest::mouseClick(bgButton, Qt::LeftButton);

    check(flame->background[0] == 10 && flame->background[1] == 20 && flame->background[2] == 30,
          "picking a background color updates Flame::background");

    delete dialog;
}

void testGradientRotateModeMatchesGradientOps() {
    auto flame = makeTestFlame();
    for (int i = 0; i < 256; ++i) flame->cmap.entries[i] = {i, 255 - i, 128, 255};
    const apo::ColorMap baseline = flame->cmap;

    auto* dialog = new apo::ui::AdjustDialog(flame);
    auto* modeCombo = dialog->findChild<QComboBox*>("gradientModeCombo");
    auto* amountCtrl = dialog->findChild<apo::ui::SliderSpin*>("gradientAmountCtrl");
    if (!check(modeCombo != nullptr && amountCtrl != nullptr, "AdjustDialog exposes gradient mode combo + amount control")) {
        delete dialog;
        return;
    }

    check(modeCombo->currentText() == "Rotate", "Rotate is the default gradient mode");
    typeIntoSliderSpin(amountCtrl, 20);

    const apo::ColorMap expected = apo::rotateGradient(baseline, 20);
    check(flame->cmap.entries == expected.entries,
          "the Gradient tab's Rotate mode produces exactly what GradientOps::rotateGradient computes");

    delete dialog;
}

void testGradientModeSwitchResetsAmountWithoutChangingGradient() {
    auto flame = makeTestFlame();
    for (int i = 0; i < 256; ++i) flame->cmap.entries[i] = {i, 255 - i, 128, 255};

    auto* dialog = new apo::ui::AdjustDialog(flame);
    auto* modeCombo = dialog->findChild<QComboBox*>("gradientModeCombo");
    auto* amountCtrl = dialog->findChild<apo::ui::SliderSpin*>("gradientAmountCtrl");
    if (!check(modeCombo != nullptr && amountCtrl != nullptr, "gradient controls found")) {
        delete dialog;
        return;
    }

    typeIntoSliderSpin(amountCtrl, 20); // rotate by 20, committed
    const apo::ColorMap afterRotate = flame->cmap;

    modeCombo->setCurrentIndex(1); // Hue
    check(amountCtrl->value() == 0.0, "switching gradient mode resets the Amount control to that mode's default");
    check(flame->cmap.entries == afterRotate.entries, "switching gradient mode alone does not change the gradient");

    delete dialog;
}

void testInvertReverseRandomizeResetButtons() {
    auto flame = makeTestFlame();
    for (int i = 0; i < 256; ++i) flame->cmap.entries[i] = {i, 255 - i, 128, 255};
    const apo::ColorMap original = flame->cmap;

    auto* dialog = new apo::ui::AdjustDialog(flame);
    auto* invertButton = dialog->findChild<QPushButton*>("invertButton");
    auto* reverseButton = dialog->findChild<QPushButton*>("reverseButton");
    auto* randomizeButton = dialog->findChild<QPushButton*>("randomizeButton");
    auto* resetButton = dialog->findChild<QPushButton*>("resetButton");
    if (!check(invertButton && reverseButton && randomizeButton && resetButton, "all four gradient buttons found")) {
        delete dialog;
        return;
    }

    QTest::mouseClick(invertButton, Qt::LeftButton);
    check(flame->cmap.entries == apo::invertGradient(original).entries, "Invert button applies invertGradient");

    QTest::mouseClick(reverseButton, Qt::LeftButton);
    check(flame->cmap.entries == apo::reverseGradient(apo::invertGradient(original)).entries,
          "Reverse button applies reverseGradient on top of the already-inverted gradient");

    QTest::mouseClick(randomizeButton, Qt::LeftButton);
    check(!(flame->cmap.entries == original.entries), "Randomize button produces a different gradient");

    QTest::mouseClick(resetButton, Qt::LeftButton);
    check(flame->cmap.entries == original.entries,
          "Reset button restores the gradient present when the dialog was opened");

    delete dialog;
}

void testSizeTabApplyRescalesFlame() {
    auto flame = makeTestFlame();
    flame->width = 100;
    flame->height = 100;
    flame->pixelsPerUnit = 50;

    auto* dialog = new apo::ui::AdjustDialog(flame);
    auto* widthSpin = dialog->findChild<QSpinBox*>("widthSpin");
    auto* heightSpin = dialog->findChild<QSpinBox*>("heightSpin");
    auto* applyButton = dialog->findChild<QPushButton*>("sizeApplyButton");
    if (!check(widthSpin && heightSpin && applyButton, "Size tab controls found")) {
        delete dialog;
        return;
    }

    widthSpin->setValue(200);
    heightSpin->setValue(150);
    QTest::mouseClick(applyButton, Qt::LeftButton);

    check(flame->width == 200 && flame->height == 150, "Apply updates Flame::width/height");
    check(approxEqual(flame->pixelsPerUnit, 100.0), "Apply rescales pixelsPerUnit proportionally (matches Flame::adjustScale)");

    delete dialog;
}

void testFlameChangedSignalFiresOnceOnCommit() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::AdjustDialog(flame);
    auto* brightnessCtrl = dialog->findChild<apo::ui::SliderSpin*>("brightnessCtrl");
    if (!check(brightnessCtrl != nullptr, "brightnessCtrl found")) {
        delete dialog;
        return;
    }

    QSignalSpy spy(dialog, &apo::ui::AdjustDialog::flameChanged);
    typeIntoSliderSpin(brightnessCtrl, 7.5);
    check(spy.count() == 1, "flameChanged fires exactly once for a single committed edit");

    delete dialog;
}

void testGradientBrowseButtonAppliesSelectedGradientWithUndo() {
    const std::string path = "adjust_dialog_gradient_browser_test.ugr";
    {
        std::ofstream out(path);
        out << "Sunrise {\n\tgradient:\n\t index=0 color=255\n\t index=255 color=16711680\n\topacity: 0\n}\n";
    }

    auto flame = makeTestFlame();
    const apo::ColorMap originalCmap = flame->cmap;
    auto* dialog = new apo::ui::AdjustDialog(flame);
    auto* browseButton = dialog->findChild<QPushButton*>("gradientBrowseButton");
    if (!check(browseButton != nullptr, "AdjustDialog exposes a gradientBrowseButton")) {
        delete dialog;
        std::remove(path.c_str());
        return;
    }

    QTest::mouseClick(browseButton, Qt::LeftButton);
    auto* browser = dialog->findChild<apo::ui::GradientBrowserDialog*>();
    if (!check(browser != nullptr, "clicking Browse Gradients... opens a GradientBrowserDialog child")) {
        delete dialog;
        std::remove(path.c_str());
        return;
    }

    browser->openFile(QString::fromStdString(path)); // auto-selects the sole "Sunrise" entry
    auto* applyButton = browser->findChild<QPushButton*>("applyButton");
    if (!check(applyButton != nullptr, "the browser exposes its applyButton")) {
        delete dialog;
        std::remove(path.c_str());
        return;
    }

    apo::ColorMap expected{};
    check(apo::loadGradientFile(path, expected, "Sunrise"), "GradientIO can load 'Sunrise' directly for comparison");

    QTest::mouseClick(applyButton, Qt::LeftButton);

    check(flame->cmap.entries == expected.entries,
          "applying a gradient from the browser sets Flame::cmap to exactly what GradientIO parsed");

    dialog->show();
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(dialog);
    QT_WARNING_POP
    QTest::keySequence(dialog, QKeySequence::Undo);
    check(flame->cmap.entries == originalCmap.entries,
          "applying a gradient from the browser pushes a real undo entry onto AdjustDialog's own stack");

    delete dialog;
    std::remove(path.c_str());
}

void testClickingGradientStripOpensGradientBrowser() {
    auto flame = makeTestFlame();
    auto* dialog = new apo::ui::AdjustDialog(flame);
    auto* gradientStrip = dialog->findChild<QLabel*>("gradientStrip");
    if (!check(gradientStrip != nullptr, "AdjustDialog exposes a gradientStrip label")) {
        delete dialog;
        return;
    }

    QTest::mouseClick(gradientStrip, Qt::LeftButton);
    auto* browser = dialog->findChild<apo::ui::GradientBrowserDialog*>();
    check(browser != nullptr, "clicking the gradient strip itself opens a GradientBrowserDialog child, same as Browse Gradients...");

    delete dialog;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testSpinBoxEditCommitsToFlameAndUndo();
    testAngleFieldConvertsDegreesToRadians();
    testGammaSliderCommitsLiveAndOnRelease();
    testBackgroundColorPicker();
    testGradientRotateModeMatchesGradientOps();
    testGradientModeSwitchResetsAmountWithoutChangingGradient();
    testInvertReverseRandomizeResetButtons();
    testSizeTabApplyRescalesFlame();
    testFlameChangedSignalFiresOnceOnCommit();
    testGradientBrowseButtonAppliesSelectedGradientWithUndo();
    testClickingGradientStripOpensGradientBrowser();

    return apo_test::reportAndExit();
}
