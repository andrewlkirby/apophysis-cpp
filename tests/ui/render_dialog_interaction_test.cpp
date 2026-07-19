// Real-widget interaction tests for RenderDialog (src/ui/RenderDialog.h) -
// apo::Renderer's cooperative-cancellation/progress-reporting mechanism
// (RenderProgress) is already covered independently in renderer_test.cpp;
// this file verifies the Qt wiring on top of it: controls reflect the
// flame's initial settings, maintain-aspect-ratio sync, the Render button
// is gated on an output path, Browse opens a real (offscreen-backed, not
// native) QFileDialog, a real click-to-Render actually produces a decodable
// PNG file on disk with the requested dimensions, and Cancel stops an
// in-flight render and is reflected in the status text. Mirrors
// adjust_dialog_interaction_test.cpp's approach (QTest event injection
// against the real compiled widget, via the apo_ui object library).

#include <cstddef>
#include <cstdio>
#include <functional>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>
#include <QTimer>

#include "../TestHelpers.h"
#include "RenderDialog.h"
#include "core/io/FlameIO.h"
#include "core/io/PngWriter.h"

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

// A classic 3-map Sierpinski-gasket IFS - same shape used in
// renderer_test.cpp - small and fast enough to fully render in a test, but
// with real chaos-game structure rather than a degenerate fixed point.
std::shared_ptr<apo::Flame> makeSierpinskiFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->width = 48;
    flame->height = 48;
    flame->sampleDensity = 20;
    flame->center = {0.0, 0.0};
    flame->pixelsPerUnit = 24;
    flame->whiteLevel = 200;
    for (int i = 0; i < 256; ++i) flame->cmap.entries[i] = {i, i, i, 255};

    const double corners[3][2] = {{-0.5, -0.5}, {0.5, -0.5}, {0.0, 0.5}};
    for (int i = 0; i < 3; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.color = i / 2.0;
        xf.c[0] = {0.5, 0.0};
        xf.c[1] = {0.0, 0.5};
        xf.c[2] = {corners[i][0], corners[i][1]};
    }
    return flame;
}

// Polls until `pred` is true or `timeoutMs` elapses, pumping the event
// loop each iteration - needed because RenderDialog's actual render runs
// asynchronously on a worker QThread, not synchronously within a slot.
bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!pred()) {
        if (timer.elapsed() > timeoutMs) return false;
        QTest::qWait(20);
    }
    return true;
}

void testControlsInitializedFromFlame() {
    auto flame = makeSierpinskiFlame();
    flame->spatialOversample = 2;
    flame->spatialFilterRadius = 0.75;
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* widthSpin = dialog->findChild<QSpinBox*>("widthSpin");
    auto* heightSpin = dialog->findChild<QSpinBox*>("heightSpin");
    auto* densitySpin = dialog->findChild<QDoubleSpinBox*>("densitySpin");
    auto* oversampleSpin = dialog->findChild<QSpinBox*>("oversampleSpin");
    auto* filterRadiusSpin = dialog->findChild<QDoubleSpinBox*>("filterRadiusSpin");
    auto* adaptiveFilterCheck = dialog->findChild<QCheckBox*>("adaptiveFilterCheck");
    auto* transparentBackgroundCheck = dialog->findChild<QCheckBox*>("transparentBackgroundCheck");
    if (!check(widthSpin && heightSpin && densitySpin && oversampleSpin && filterRadiusSpin && adaptiveFilterCheck &&
                   transparentBackgroundCheck,
               "RenderDialog exposes width/height/density/oversample/filterRadius/adaptiveFilter/"
               "transparentBackground controls")) {
        delete dialog;
        return;
    }

    check(widthSpin->value() == flame->width, "widthSpin starts at the flame's width");
    check(heightSpin->value() == flame->height, "heightSpin starts at the flame's height");
    check(approxEqual(densitySpin->value(), flame->sampleDensity), "densitySpin starts at the flame's sampleDensity");
    check(oversampleSpin->value() == flame->spatialOversample, "oversampleSpin starts at the flame's spatialOversample");
    check(approxEqual(filterRadiusSpin->value(), flame->spatialFilterRadius),
          "filterRadiusSpin starts at the flame's spatialFilterRadius");
    check(!adaptiveFilterCheck->isChecked(), "adaptiveFilterCheck starts unchecked, matching the flame's default enableDE=false");
    check(!transparentBackgroundCheck->isChecked(),
          "transparentBackgroundCheck starts unchecked, matching the flame's default transparency=false");

    delete dialog;
}

void testAdaptiveFilterCheckboxReachesTheRenderedFlame() {
    const char* outputPath = "render_dialog_de_test_output.png";
    std::remove(outputPath);

    auto flame = makeSierpinskiFlame();
    flame->enableDE = true; // dialog should start checked...
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* adaptiveFilterCheck = dialog->findChild<QCheckBox*>("adaptiveFilterCheck");
    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    auto* renderButton = dialog->findChild<QPushButton*>("renderButton");
    auto* cancelButton = dialog->findChild<QPushButton*>("cancelButton");
    if (!check(adaptiveFilterCheck && outputPathEdit && renderButton && cancelButton, "controls found")) {
        delete dialog;
        return;
    }
    check(adaptiveFilterCheck->isChecked(), "adaptiveFilterCheck starts checked, matching the flame's enableDE=true");

    // ...and unchecking it before Render should turn DE off for that
    // specific render, without touching the original flame_ (RenderDialog
    // always renders a clone - see its class comment).
    adaptiveFilterCheck->setChecked(false);
    outputPathEdit->setText(outputPath);

    QSignalSpy spy(dialog, &apo::ui::RenderDialog::fullRenderRequested);
    QTest::mouseClick(renderButton, Qt::LeftButton);

    check(spy.count() == 1, "clicking Render emits fullRenderRequested exactly once");
    if (spy.count() == 1) {
        const auto renderedFlame = spy.at(0).at(0).value<std::shared_ptr<const apo::Flame>>();
        check(renderedFlame && !renderedFlame->enableDE,
              "unchecking Adaptive Filter reaches the actual flame clone passed to the render call");
    }
    check(flame->enableDE, "unchecking the dialog's checkbox never touches the original flame_ instance");

    // Not interested in the actual output here - cancel and let it wind
    // down rather than waiting out a full render just to clean up.
    QTest::mouseClick(cancelButton, Qt::LeftButton);
    waitUntil([renderButton] { return renderButton->isEnabled(); });
    std::remove(outputPath);

    delete dialog;
}

void testTransparentBackgroundCheckboxReachesTheRenderedFlame() {
    const char* outputPath = "render_dialog_transparency_test_output.png";
    std::remove(outputPath);

    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* transparentBackgroundCheck = dialog->findChild<QCheckBox*>("transparentBackgroundCheck");
    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    auto* renderButton = dialog->findChild<QPushButton*>("renderButton");
    auto* cancelButton = dialog->findChild<QPushButton*>("cancelButton");
    if (!check(transparentBackgroundCheck && outputPathEdit && renderButton && cancelButton, "controls found")) {
        delete dialog;
        return;
    }

    transparentBackgroundCheck->setChecked(true);
    outputPathEdit->setText(outputPath);

    QSignalSpy spy(dialog, &apo::ui::RenderDialog::fullRenderRequested);
    QTest::mouseClick(renderButton, Qt::LeftButton);

    check(spy.count() == 1, "clicking Render emits fullRenderRequested exactly once");
    if (spy.count() == 1) {
        const auto renderedFlame = spy.at(0).at(0).value<std::shared_ptr<const apo::Flame>>();
        check(renderedFlame && renderedFlame->transparency,
              "checking Transparent Background reaches the actual flame clone passed to the render call");
    }
    check(!flame->transparency, "checking the dialog's checkbox never touches the original flame_ instance");

    // Not interested in the actual output here - cancel and let it wind
    // down rather than waiting out a full render just to clean up.
    QTest::mouseClick(cancelButton, Qt::LeftButton);
    waitUntil([renderButton] { return renderButton->isEnabled(); });
    std::remove(outputPath);

    delete dialog;
}

void testTransparentBackgroundProducesRealAlphaInOutputPng() {
    const char* outputPath = "render_dialog_transparency_alpha_test_output.png";
    std::remove(outputPath);

    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* widthSpin = dialog->findChild<QSpinBox*>("widthSpin");
    auto* heightSpin = dialog->findChild<QSpinBox*>("heightSpin");
    auto* densitySpin = dialog->findChild<QDoubleSpinBox*>("densitySpin");
    auto* transparentBackgroundCheck = dialog->findChild<QCheckBox*>("transparentBackgroundCheck");
    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    auto* renderButton = dialog->findChild<QPushButton*>("renderButton");
    if (!check(widthSpin && heightSpin && densitySpin && transparentBackgroundCheck && outputPathEdit && renderButton,
               "controls needed for the transparency-output test found")) {
        delete dialog;
        return;
    }

    widthSpin->setValue(40);
    heightSpin->setValue(30);
    densitySpin->setValue(15);
    transparentBackgroundCheck->setChecked(true);
    outputPathEdit->setText(outputPath);

    QTest::mouseClick(renderButton, Qt::LeftButton);
    const bool finished = waitUntil([renderButton] { return renderButton->isEnabled(); });
    if (!check(finished, "the full render completes within the timeout")) {
        delete dialog;
        return;
    }

    int outWidth = 0, outHeight = 0;
    std::vector<std::uint8_t> pixels;
    const bool loaded = apo::readPng(outputPath, outWidth, outHeight, pixels);
    if (!check(loaded, "the transparent render still writes a decodable PNG file")) {
        delete dialog;
        return;
    }

    // readPng() always decodes to RGBA, defaulting alpha to 255 for an
    // opaque (3-channel) source - see its own doc comment - so seeing any
    // alpha < 255 at all only happens for a genuinely-written 4-channel
    // PNG, i.e. proof Flame::transparency actually reached the renderer,
    // not just that the checkbox itself got set. Checking min/max across
    // the whole image (rather than one specific pixel) is deliberate: the
    // Sierpinski fixture's chaos-game-filled region plus its filter-radius
    // bleed doesn't align to a single guessable pixel, but *some* pixels
    // (untouched canvas far from the triangle) must stay at the renderer's
    // untouched-pixel alpha=0 default (see Renderer.cpp's `int
    // r=0,g=0,b=0,a=0`, only overwritten by the background fill when
    // transparency is off) while others (inside the triangle) are opaque.
    int minAlpha = 255, maxAlpha = 0;
    for (size_t i = 3; i < pixels.size(); i += 4) {
        minAlpha = std::min<int>(minAlpha, pixels[i]);
        maxAlpha = std::max<int>(maxAlpha, pixels[i]);
    }
    check(minAlpha == 0,
          "at least one untouched pixel is fully transparent (alpha=0) when Transparent Background is checked");
    check(maxAlpha == 255, "at least one pixel inside the rendered shape is fully opaque (alpha=255)");

    std::remove(outputPath);
    delete dialog;
}

void testMemoryEstimateLabelUpdatesWithSizeAndOversampleNotDensity() {
    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* widthSpin = dialog->findChild<QSpinBox*>("widthSpin");
    auto* oversampleSpin = dialog->findChild<QSpinBox*>("oversampleSpin");
    auto* densitySpin = dialog->findChild<QDoubleSpinBox*>("densitySpin");
    auto* memoryLabel = dialog->findChild<QLabel*>("memoryEstimateLabel");
    if (!check(widthSpin && oversampleSpin && densitySpin && memoryLabel,
               "width/oversample/density spins and memoryEstimateLabel found")) {
        delete dialog;
        return;
    }

    const QString initialText = memoryLabel->text();
    check(!initialText.isEmpty(), "the memory estimate label starts populated (not blank) as soon as the dialog opens");

    widthSpin->setValue(widthSpin->value() * 4);
    check(memoryLabel->text() != initialText, "raising the width updates the memory estimate");

    const QString afterWidthText = memoryLabel->text();
    oversampleSpin->setValue(oversampleSpin->value() + 1);
    check(memoryLabel->text() != afterWidthText, "raising oversample updates the memory estimate too");

    const QString afterOversampleText = memoryLabel->text();
    densitySpin->setValue(densitySpin->value() * 100);
    check(memoryLabel->text() == afterOversampleText,
          "sample density does not affect the memory estimate - it changes point count, not bucket array size");

    delete dialog;
}

void testWidthPresetComboPopulatesSpinAndSyncsHeight() {
    auto flame = makeSierpinskiFlame();
    flame->width = 100;
    flame->height = 50; // 2:1 aspect
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* widthSpin = dialog->findChild<QSpinBox*>("widthSpin");
    auto* heightSpin = dialog->findChild<QSpinBox*>("heightSpin");
    auto* widthPresetCombo = dialog->findChild<QComboBox*>("widthPresetCombo");
    if (!check(widthSpin && heightSpin && widthPresetCombo, "widthSpin/heightSpin/widthPresetCombo found")) {
        delete dialog;
        return;
    }
    check(widthPresetCombo->currentText() == "Preset...", "the preset combo starts on its placeholder, not a real value");

    dialog->show();
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(dialog);
    QT_WARNING_POP

    const int index1024 = widthPresetCombo->findText("1024");
    if (!check(index1024 > 0, "1024 is one of the width presets, matching FormRender.dfm's cbWidth exactly")) {
        delete dialog;
        return;
    }
    widthPresetCombo->setCurrentIndex(index1024);
    widthPresetCombo->activated(index1024); // simulates the user picking this item, not just a programmatic index change

    check(widthSpin->value() == 1024, "picking a width preset writes its value into widthSpin");
    check(heightSpin->value() == 512,
          "picking a width preset also re-syncs height under Maintain Aspect Ratio, exactly like typing the "
          "value in by hand would");

    delete dialog;
}

void testDensityPresetComboWritesIntoDensitySpin() {
    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* densitySpin = dialog->findChild<QDoubleSpinBox*>("densitySpin");
    auto* densityPresetCombo = dialog->findChild<QComboBox*>("densityPresetCombo");
    if (!check(densitySpin && densityPresetCombo, "densitySpin and densityPresetCombo found")) {
        delete dialog;
        return;
    }

    const int index2000 = densityPresetCombo->findText("2000");
    if (!check(index2000 > 0, "2000 is one of the density presets, matching FormRender.dfm's txtDensity exactly")) {
        delete dialog;
        return;
    }
    densityPresetCombo->setCurrentIndex(index2000);
    densityPresetCombo->activated(index2000);

    check(approxEqual(densitySpin->value(), 2000.0), "picking a density preset writes its value into densitySpin");

    delete dialog;
}

void testPresetCombosAreDisabledWhileARenderIsInFlight() {
    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    auto* renderButton = dialog->findChild<QPushButton*>("renderButton");
    auto* widthPresetCombo = dialog->findChild<QComboBox*>("widthPresetCombo");
    if (!check(outputPathEdit && renderButton && widthPresetCombo, "controls found")) {
        delete dialog;
        return;
    }

    const char* outputPath = "render_dialog_preset_disabled_test_output.png";
    std::remove(outputPath);
    outputPathEdit->setText(outputPath);
    check(widthPresetCombo->isEnabled(), "widthPresetCombo starts enabled before any render");

    QTest::mouseClick(renderButton, Qt::LeftButton);
    check(!widthPresetCombo->isEnabled(), "widthPresetCombo is disabled while a render is in flight, like every other input control");

    QTest::mouseClick(dialog->findChild<QPushButton*>("cancelButton"), Qt::LeftButton);
    waitUntil([renderButton] { return renderButton->isEnabled(); });
    check(widthPresetCombo->isEnabled(), "widthPresetCombo is re-enabled once the render finishes");

    std::remove(outputPath);
    delete dialog;
}

void testMaintainAspectRatioSyncsHeightFromWidth() {
    auto flame = makeSierpinskiFlame();
    flame->width = 100;
    flame->height = 50;
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* widthSpin = dialog->findChild<QSpinBox*>("widthSpin");
    auto* heightSpin = dialog->findChild<QSpinBox*>("heightSpin");
    auto* maintainAspectCheck = dialog->findChild<QCheckBox*>("maintainAspectCheck");
    if (!check(widthSpin && heightSpin && maintainAspectCheck, "Size controls found")) {
        delete dialog;
        return;
    }

    check(maintainAspectCheck->isChecked(), "maintain-aspect-ratio starts checked");

    // hasFocus()-gated sync (see RenderDialog.cpp) needs the dialog
    // actually active on the offscreen QPA platform - same
    // deprecated-but-synchronous QApplication::setActiveWindow() trick
    // adjust_dialog_interaction_test.cpp uses, since
    // QWidget::activateWindow() doesn't update activation state
    // synchronously there.
    dialog->show();
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(dialog);
    QT_WARNING_POP

    widthSpin->setFocus();
    widthSpin->setValue(200);
    check(heightSpin->value() == 100, "raising width to 200 (2x) proportionally updates height to 100 (2:1 aspect)");

    maintainAspectCheck->setChecked(false);
    heightSpin->setFocus();
    heightSpin->setValue(300);
    check(widthSpin->value() == 200, "unchecking maintain-aspect stops width from following height changes");

    delete dialog;
}

void testMaintainAspectRatioRecapturesRatioWhenReChecked() {
    auto flame = makeSierpinskiFlame();
    flame->width = 100;
    flame->height = 50; // 2:1 - deliberately different from the custom size typed below
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* widthSpin = dialog->findChild<QSpinBox*>("widthSpin");
    auto* heightSpin = dialog->findChild<QSpinBox*>("heightSpin");
    auto* maintainAspectCheck = dialog->findChild<QCheckBox*>("maintainAspectCheck");
    if (!check(widthSpin && heightSpin && maintainAspectCheck, "Size controls found")) {
        delete dialog;
        return;
    }

    dialog->show();
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(dialog);
    QT_WARNING_POP

    // Type a custom size unrelated to the flame's original 2:1 ratio while
    // unchecked, then check the box - it should capture *this* ratio (3:1),
    // not silently fall back to the stale ratio from dialog-open time.
    maintainAspectCheck->setChecked(false);
    widthSpin->setFocus();
    widthSpin->setValue(300);
    heightSpin->setFocus();
    heightSpin->setValue(100);

    maintainAspectCheck->setChecked(true);
    widthSpin->setFocus();
    widthSpin->setValue(600);
    check(heightSpin->value() == 200,
          "re-checking maintain-aspect after a custom size recaptures that size's own 3:1 ratio, not the flame's "
          "original 2:1 ratio from when the dialog opened");

    delete dialog;
}

void testRenderButtonDisabledUntilOutputPathSet() {
    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* renderButton = dialog->findChild<QPushButton*>("renderButton");
    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    if (!check(renderButton && outputPathEdit, "Render button and output path edit found")) {
        delete dialog;
        return;
    }

    check(!renderButton->isEnabled(), "Render is disabled with no output path chosen");
    outputPathEdit->setText("render_dialog_test_output.png");
    check(renderButton->isEnabled(), "Render becomes enabled once an output path is typed in");
    outputPathEdit->clear();
    check(!renderButton->isEnabled(), "Render becomes disabled again if the output path is cleared");

    delete dialog;
}

void testBrowseOpensFileDialogAndFillsPath() {
    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);
    auto* browseButton = dialog->findChild<QPushButton*>("browseButton");
    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    if (!check(browseButton && outputPathEdit, "Browse button and output path edit found")) {
        delete dialog;
        return;
    }

    // QFileDialog::getSaveFileName() runs its own nested modal event loop
    // on the (native-dialog-less) offscreen QPA platform, using Qt's own
    // QFileDialog widget - the same modal-dialog-testing pattern
    // adjust_dialog_interaction_test.cpp uses for QColorDialog.
    whenModalShown<QFileDialog>(dialog, [](QFileDialog* fileDialog) {
        fileDialog->selectFile("browsed_output.png");
        // QFileDialog re-declares accept()/done() as protected, but access
        // control is checked against the *static* type at the call site,
        // not the dynamic override - calling through a QDialog* (where
        // both are public) still virtually dispatches to QFileDialog's own
        // override.
        static_cast<QDialog*>(fileDialog)->accept();
    });
    QTest::mouseClick(browseButton, Qt::LeftButton);

    check(outputPathEdit->text().endsWith("browsed_output.png"),
          "Browse... fills the output path edit with the chosen file");

    delete dialog;
}

void testFullRenderProducesRealPngFile() {
    const char* outputPath = "render_dialog_test_output.png";
    std::remove(outputPath);

    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* widthSpin = dialog->findChild<QSpinBox*>("widthSpin");
    auto* heightSpin = dialog->findChild<QSpinBox*>("heightSpin");
    auto* densitySpin = dialog->findChild<QDoubleSpinBox*>("densitySpin");
    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    auto* renderButton = dialog->findChild<QPushButton*>("renderButton");
    auto* cancelButton = dialog->findChild<QPushButton*>("cancelButton");
    auto* progressBar = dialog->findChild<QProgressBar*>("progressBar");
    auto* statusLabel = dialog->findChild<QLabel*>("statusLabel");
    if (!check(widthSpin && heightSpin && densitySpin && outputPathEdit && renderButton && cancelButton &&
                   progressBar && statusLabel,
               "all controls needed for a full render found")) {
        delete dialog;
        return;
    }

    widthSpin->setValue(40);
    heightSpin->setValue(30);
    densitySpin->setValue(15);
    outputPathEdit->setText(outputPath);

    QTest::mouseClick(renderButton, Qt::LeftButton);
    check(cancelButton->isEnabled(), "clicking Render immediately enables Cancel (a render is now in flight)");
    check(!renderButton->isEnabled(), "clicking Render immediately disables Render itself (no re-entrant renders)");

    // renderButton (not cancelButton) is the reliable "truly finished"
    // signal: cancelRender() disables cancelButton_ the instant Cancel is
    // clicked, independent of whether the worker thread has actually
    // finished yet (see the cancel test below), but renderButton_ is only
    // re-enabled from setControlsEnabled(true), called exclusively from
    // onFullRenderFinished.
    const bool finished = waitUntil([renderButton] { return renderButton->isEnabled(); });
    check(finished, "the full render completes within the timeout");
    check(!cancelButton->isEnabled(), "Cancel is disabled again once the render finishes");
    check(progressBar->value() == 100, "the progress bar reaches 100% on a completed, uncancelled render");
    check(statusLabel->text().contains("Done"), "the status label reports completion");

    int outWidth = 0, outHeight = 0;
    std::vector<std::uint8_t> pixels;
    const bool loaded = apo::readPng(outputPath, outWidth, outHeight, pixels);
    check(loaded, "RenderDialog's Render button writes a real, decodable PNG file to the chosen path");
    check(outWidth == 40 && outHeight == 30, "the saved PNG has the dimensions requested in the Width/Height spins");

    std::remove(outputPath);
    delete dialog;
}

void testSaveParametersWritesAMatchingFlameFileAlongsideThePng() {
    const char* outputPath = "render_dialog_save_params_test_output.png";
    const char* flamePath = "render_dialog_save_params_test_output.flame";
    std::remove(outputPath);
    std::remove(flamePath);

    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* widthSpin = dialog->findChild<QSpinBox*>("widthSpin");
    auto* heightSpin = dialog->findChild<QSpinBox*>("heightSpin");
    auto* oversampleSpin = dialog->findChild<QSpinBox*>("oversampleSpin");
    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    auto* saveParamsCheck = dialog->findChild<QCheckBox*>("saveParametersCheck");
    auto* renderButton = dialog->findChild<QPushButton*>("renderButton");
    if (!check(widthSpin && heightSpin && oversampleSpin && outputPathEdit && saveParamsCheck && renderButton,
               "controls needed for the save-parameters test found")) {
        delete dialog;
        return;
    }
    check(!saveParamsCheck->isChecked(), "Save Parameters starts unchecked");

    widthSpin->setValue(40);
    heightSpin->setValue(30);
    oversampleSpin->setValue(2); // deliberately different from the flame's own original oversample
    outputPathEdit->setText(outputPath);
    saveParamsCheck->setChecked(true);

    QTest::mouseClick(renderButton, Qt::LeftButton);
    const bool finished = waitUntil([renderButton] { return renderButton->isEnabled(); });
    if (!check(finished, "the full render completes within the timeout")) {
        delete dialog;
        return;
    }

    auto loaded = apo::loadFlameFile(flamePath);
    if (check(!loaded.empty(), "checking Save Parameters writes a loadable .flame file next to the PNG")) {
        check(loaded.front()->width == 40 && loaded.front()->height == 30,
              "the saved .flame reflects the dialog's own width/height at Render time, not the original flame_'s");
        check(loaded.front()->spatialOversample == 2,
              "the saved .flame reflects the dialog's own oversample at Render time");
    }

    std::remove(outputPath);
    std::remove(flamePath);
    delete dialog;
}

void testWithoutSaveParametersNoFlameFileIsWritten() {
    const char* outputPath = "render_dialog_no_save_params_test_output.png";
    const char* flamePath = "render_dialog_no_save_params_test_output.flame";
    std::remove(outputPath);
    std::remove(flamePath);

    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    auto* saveParamsCheck = dialog->findChild<QCheckBox*>("saveParametersCheck");
    auto* renderButton = dialog->findChild<QPushButton*>("renderButton");
    if (!check(outputPathEdit && saveParamsCheck && renderButton, "controls found")) {
        delete dialog;
        return;
    }

    outputPathEdit->setText(outputPath);
    QTest::mouseClick(renderButton, Qt::LeftButton);
    const bool finished = waitUntil([renderButton] { return renderButton->isEnabled(); });
    if (!check(finished, "the full render completes within the timeout")) {
        delete dialog;
        return;
    }

    auto loaded = apo::loadFlameFile(flamePath);
    check(loaded.empty(), "leaving Save Parameters unchecked (the default) writes no .flame file at all");

    std::remove(outputPath);
    std::remove(flamePath);
    delete dialog;
}

void testPauseFreezesProgressUntilResumed() {
    const char* outputPath = "render_dialog_pause_test_output.png";
    std::remove(outputPath);

    auto flame = makeSierpinskiFlame();
    flame->sampleDensity = 2000; // enough sub-batches that pausing has room to matter
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    auto* renderButton = dialog->findChild<QPushButton*>("renderButton");
    auto* pauseButton = dialog->findChild<QPushButton*>("pauseButton");
    auto* progressBar = dialog->findChild<QProgressBar*>("progressBar");
    auto* statusLabel = dialog->findChild<QLabel*>("statusLabel");
    if (!check(outputPathEdit && renderButton && pauseButton && progressBar && statusLabel,
               "controls needed for the pause test found")) {
        delete dialog;
        return;
    }
    check(!pauseButton->isEnabled(), "Pause starts disabled - no render is in flight yet");

    outputPathEdit->setText(outputPath);

    // Same back-to-back trick testCancelStopsRenderAndUpdatesStatus relies
    // on: clicking Pause with no event-loop turn after Render means
    // pauseRequested is already set before the worker thread's first
    // per-sub-batch check, so the render blocks deterministically after
    // exactly one 10,000-point sub-batch.
    QTest::mouseClick(renderButton, Qt::LeftButton);
    QTest::mouseClick(pauseButton, Qt::LeftButton);
    check(pauseButton->text() == "Resume", "clicking Pause immediately flips the button to Resume");
    check(pauseButton->isEnabled(), "Pause/Resume stays enabled while paused - it's how you unpause");

    // Give the worker thread a moment to actually reach the pause point and
    // report its progress back, then confirm it's genuinely frozen there.
    QTest::qWait(200);
    check(statusLabel->text().contains("Paused"), "the status label reports the paused state");
    const int percentWhilePaused = progressBar->value();
    QTest::qWait(200);
    check(progressBar->value() == percentWhilePaused, "progress stays frozen for as long as the render stays paused");
    check(!renderButton->isEnabled(), "a paused render is still \"in flight\", not finished");

    QTest::mouseClick(pauseButton, Qt::LeftButton);
    check(pauseButton->text() == "Pause", "clicking Resume flips the button back to Pause");

    const bool finished = waitUntil([renderButton] { return renderButton->isEnabled(); });
    check(finished, "resuming lets the render reach completion within the timeout");
    check(statusLabel->text().contains("Done"), "the status label reports completion after a pause/resume cycle");
    check(!pauseButton->isEnabled(), "Pause is disabled again once the render finishes");
    check(pauseButton->text() == "Pause", "Pause's label is reset back to \"Pause\" once the render finishes");

    int outWidth = 0, outHeight = 0;
    std::vector<std::uint8_t> pixels;
    const bool loaded = apo::readPng(outputPath, outWidth, outHeight, pixels);
    check(loaded && outWidth == flame->width && outHeight == flame->height,
          "a paused-then-resumed render still produces a real, correctly-sized PNG");

    std::remove(outputPath);
    delete dialog;
}

void testPauseThenCancelStillCancels() {
    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    auto* renderButton = dialog->findChild<QPushButton*>("renderButton");
    auto* pauseButton = dialog->findChild<QPushButton*>("pauseButton");
    auto* cancelButton = dialog->findChild<QPushButton*>("cancelButton");
    auto* statusLabel = dialog->findChild<QLabel*>("statusLabel");
    if (!check(outputPathEdit && renderButton && pauseButton && cancelButton && statusLabel,
               "controls needed for the pause-then-cancel test found")) {
        delete dialog;
        return;
    }

    const char* outputPath = "render_dialog_pause_then_cancel_test_output.png";
    std::remove(outputPath);
    outputPathEdit->setText(outputPath);

    QTest::mouseClick(renderButton, Qt::LeftButton);
    QTest::mouseClick(pauseButton, Qt::LeftButton);
    QTest::mouseClick(cancelButton, Qt::LeftButton);
    check(!pauseButton->isEnabled(), "Cancel disables Pause too, without needing Resume clicked first");

    const bool finished = waitUntil([renderButton] { return renderButton->isEnabled(); });
    check(finished, "a render cancelled while paused still reaches a finished state within the timeout - it doesn't hang");
    check(statusLabel->text().contains("Cancelled"), "the status label reports the render was cancelled");

    std::remove(outputPath);
    delete dialog;
}

void testCancelStopsRenderAndUpdatesStatus() {
    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::RenderDialog(flame);

    auto* outputPathEdit = dialog->findChild<QLineEdit*>("outputPathEdit");
    auto* renderButton = dialog->findChild<QPushButton*>("renderButton");
    auto* cancelButton = dialog->findChild<QPushButton*>("cancelButton");
    auto* statusLabel = dialog->findChild<QLabel*>("statusLabel");
    if (!check(outputPathEdit && renderButton && cancelButton && statusLabel, "controls needed for cancel test found")) {
        delete dialog;
        return;
    }

    const char* outputPath = "render_dialog_cancel_test_output.png";
    std::remove(outputPath);
    outputPathEdit->setText(outputPath);

    // Click Render then Cancel back-to-back, with no event-loop turn in
    // between - progress_->cancelRequested is set before the worker thread
    // has had any real chance to make headway, the same
    // pre-set-before-render pattern renderer_test.cpp's
    // testCancellationStopsRenderEarly relies on.
    QTest::mouseClick(renderButton, Qt::LeftButton);
    QTest::mouseClick(cancelButton, Qt::LeftButton);
    check(!cancelButton->isEnabled(), "clicking Cancel immediately disables it, before the worker even responds");

    const bool finished = waitUntil([renderButton] { return renderButton->isEnabled(); });
    check(finished, "a cancelled render still reaches a finished state within the timeout");
    check(statusLabel->text().contains("Cancelled"), "the status label reports the render was cancelled");
    check(renderButton->isEnabled(), "Render is re-enabled again after a cancelled render");

    std::remove(outputPath);
    delete dialog;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testControlsInitializedFromFlame();
    testAdaptiveFilterCheckboxReachesTheRenderedFlame();
    testTransparentBackgroundCheckboxReachesTheRenderedFlame();
    testTransparentBackgroundProducesRealAlphaInOutputPng();
    testMemoryEstimateLabelUpdatesWithSizeAndOversampleNotDensity();
    testWidthPresetComboPopulatesSpinAndSyncsHeight();
    testDensityPresetComboWritesIntoDensitySpin();
    testPresetCombosAreDisabledWhileARenderIsInFlight();
    testMaintainAspectRatioSyncsHeightFromWidth();
    testMaintainAspectRatioRecapturesRatioWhenReChecked();
    testRenderButtonDisabledUntilOutputPathSet();
    testBrowseOpensFileDialogAndFillsPath();
    testFullRenderProducesRealPngFile();
    testSaveParametersWritesAMatchingFlameFileAlongsideThePng();
    testWithoutSaveParametersNoFlameFileIsWritten();
    testPauseFreezesProgressUntilResumed();
    testPauseThenCancelStillCancels();
    testCancelStopsRenderAndUpdatesStatus();

    return apo_test::reportAndExit();
}
