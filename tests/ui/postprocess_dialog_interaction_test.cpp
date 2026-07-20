// Real-widget interaction tests for PostProcessDialog
// (src/ui/PostProcessDialog.h) - the underlying tone-mapping math
// (Renderer's k1/k2 contrast/brightness/gamma terms) is already covered
// independently in renderer_test.cpp; this file verifies the Qt wiring on
// top of it: controls start from the flame's own values, editing them (and
// picking a background color) never writes back to the caller's shared
// Flame - the dialog's defining structural property, see
// PostProcessDialog.h's class comment - Reset restores the dialog's own
// working copy, and Save produces a real, decodable PNG reflecting the
// dialog's current settings. Mirrors render_dialog_interaction_test.cpp's
// approach for the file-dialog and async-full-render parts.

#include <cstdio>
#include <functional>
#include <memory>

#include <QApplication>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTest>
#include <QTimer>

#include "../TestHelpers.h"
#include "PostProcessDialog.h"
#include "SliderSpin.h"
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
void whenModalShown(QObject* context, F onShown, int timeoutMs = 20000) {
    auto* timer = new QTimer(context);
    QElapsedTimer elapsed;
    elapsed.start();
    timer->setInterval(10);
    auto tickCount = std::make_shared<int>(0);
    QObject::connect(timer, &QTimer::timeout, context, [timer, elapsed, onShown, timeoutMs, tickCount]() mutable {
        auto* activeModal = QApplication::activeModalWidget();
        if (++*tickCount <= 50) {
            std::fprintf(stderr, "[diag] whenModalShown<%s> tick %d @ %lldms: activeModalWidget=%s\n",
                         DialogT::staticMetaObject.className(), *tickCount, static_cast<long long>(elapsed.elapsed()),
                         activeModal ? activeModal->metaObject()->className() : "nullptr");
            std::fflush(stderr);
        }
        if (auto* dialog = qobject_cast<DialogT*>(activeModal)) {
            timer->stop();
            timer->deleteLater();
            onShown(dialog);
        } else if (elapsed.elapsed() > timeoutMs) {
            std::fprintf(stderr, "[diag] whenModalShown<%s> gave up after %lldms; activeModalWidget=%s\n",
                         DialogT::staticMetaObject.className(), static_cast<long long>(elapsed.elapsed()),
                         activeModal ? activeModal->metaObject()->className() : "nullptr");
            std::fflush(stderr);
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start();
}

std::shared_ptr<apo::Flame> makeSierpinskiFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->width = 48;
    flame->height = 36;
    flame->sampleDensity = 20;
    flame->center = {0.0, 0.0};
    flame->pixelsPerUnit = 24;
    flame->whiteLevel = 200;
    flame->gamma = 4.0;
    flame->brightness = 4.0;
    flame->contrast = 1.0;
    flame->vibrancy = 1.0;
    flame->spatialFilterRadius = 0.5;
    flame->background = {0, 0, 0, 0};
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

bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!pred()) {
        if (timer.elapsed() > timeoutMs) return false;
        QTest::qWait(20);
    }
    return true;
}

// Types a value into a SliderSpin's spin box - real end-user path, same
// helper as adjust_dialog_interaction_test.cpp's typeIntoSliderSpin (no
// Enter key needed here since PostProcessDialog has no editingFinished-vs-
// valueChanged distinction to exercise - every edit applies live).
void setSliderSpin(apo::ui::SliderSpin* ctrl, double value) {
    ctrl->findChild<QDoubleSpinBox*>()->setValue(value);
}

void testControlsStartFromFlameValues() {
    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::PostProcessDialog(flame);

    auto* filterRadiusCtrl = dialog->findChild<apo::ui::SliderSpin*>("filterRadiusCtrl");
    auto* gammaCtrl = dialog->findChild<apo::ui::SliderSpin*>("gammaCtrl");
    auto* brightnessCtrl = dialog->findChild<apo::ui::SliderSpin*>("brightnessCtrl");
    auto* contrastCtrl = dialog->findChild<apo::ui::SliderSpin*>("contrastCtrl");
    auto* vibrancyCtrl = dialog->findChild<apo::ui::SliderSpin*>("vibrancyCtrl");
    auto* backgroundButton = dialog->findChild<QPushButton*>("backgroundButton");
    auto* resetButton = dialog->findChild<QPushButton*>("resetButton");
    auto* saveButton = dialog->findChild<QPushButton*>("saveButton");
    auto* progressBar = dialog->findChild<QProgressBar*>("progressBar");
    auto* statusLabel = dialog->findChild<QLabel*>("statusLabel");
    if (!check(filterRadiusCtrl && gammaCtrl && brightnessCtrl && contrastCtrl && vibrancyCtrl && backgroundButton &&
                   resetButton && saveButton && progressBar && statusLabel,
               "PostProcessDialog exposes all five tone sliders plus background/reset/save/progress/status")) {
        delete dialog;
        return;
    }

    check(approxEqual(filterRadiusCtrl->value(), flame->spatialFilterRadius), "filterRadiusCtrl starts at the flame's spatialFilterRadius");
    check(approxEqual(gammaCtrl->value(), flame->gamma), "gammaCtrl starts at the flame's gamma");
    check(approxEqual(brightnessCtrl->value(), flame->brightness), "brightnessCtrl starts at the flame's brightness");
    check(approxEqual(contrastCtrl->value(), flame->contrast), "contrastCtrl starts at the flame's contrast");
    check(approxEqual(vibrancyCtrl->value(), flame->vibrancy), "vibrancyCtrl starts at the flame's vibrancy");
    check(statusLabel->text() == "Ready", "status label starts at 'Ready'");
    check(progressBar->value() == 0, "progress bar starts at 0");

    delete dialog;
}

void testEditingControlsNeverTouchesSharedFlame() {
    auto flame = makeSierpinskiFlame();
    const double originalGamma = flame->gamma;
    const double originalBrightness = flame->brightness;
    const double originalContrast = flame->contrast;
    const double originalVibrancy = flame->vibrancy;
    const double originalFilterRadius = flame->spatialFilterRadius;
    const auto originalBackground = flame->background;

    auto* dialog = new apo::ui::PostProcessDialog(flame);
    auto* filterRadiusCtrl = dialog->findChild<apo::ui::SliderSpin*>("filterRadiusCtrl");
    auto* gammaCtrl = dialog->findChild<apo::ui::SliderSpin*>("gammaCtrl");
    auto* brightnessCtrl = dialog->findChild<apo::ui::SliderSpin*>("brightnessCtrl");
    auto* contrastCtrl = dialog->findChild<apo::ui::SliderSpin*>("contrastCtrl");
    auto* vibrancyCtrl = dialog->findChild<apo::ui::SliderSpin*>("vibrancyCtrl");
    auto* backgroundButton = dialog->findChild<QPushButton*>("backgroundButton");
    if (!check(filterRadiusCtrl && gammaCtrl && brightnessCtrl && contrastCtrl && vibrancyCtrl && backgroundButton,
               "controls found")) {
        delete dialog;
        return;
    }

    setSliderSpin(filterRadiusCtrl, 1.5);
    setSliderSpin(gammaCtrl, 8.0);
    setSliderSpin(brightnessCtrl, 50.0);
    setSliderSpin(contrastCtrl, 5.0);
    setSliderSpin(vibrancyCtrl, 3.0);

    whenModalShown<QColorDialog>(dialog, [](QColorDialog* colorDialog) {
        colorDialog->setCurrentColor(QColor(200, 100, 50));
        colorDialog->accept();
    });
    QTest::mouseClick(backgroundButton, Qt::LeftButton);

    check(approxEqual(flame->gamma, originalGamma) && approxEqual(flame->brightness, originalBrightness) &&
              approxEqual(flame->contrast, originalContrast) && approxEqual(flame->vibrancy, originalVibrancy) &&
              approxEqual(flame->spatialFilterRadius, originalFilterRadius) && flame->background == originalBackground,
          "editing every tone control and picking a background color never writes back to the caller's shared "
          "Flame - PostProcessDialog owns a private clone (see its class comment)");

    delete dialog;
}

void testResetRestoresSlidersToInitialValues() {
    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::PostProcessDialog(flame);
    auto* gammaCtrl = dialog->findChild<apo::ui::SliderSpin*>("gammaCtrl");
    auto* contrastCtrl = dialog->findChild<apo::ui::SliderSpin*>("contrastCtrl");
    auto* resetButton = dialog->findChild<QPushButton*>("resetButton");
    if (!check(gammaCtrl && contrastCtrl && resetButton, "gammaCtrl, contrastCtrl, and resetButton found")) {
        delete dialog;
        return;
    }

    const double initialGamma = gammaCtrl->value();
    const double initialContrast = contrastCtrl->value();

    setSliderSpin(gammaCtrl, 9.0);
    setSliderSpin(contrastCtrl, 6.0);
    check(approxEqual(gammaCtrl->value(), 9.0) && approxEqual(contrastCtrl->value(), 6.0), "edits reached the sliders");

    QTest::mouseClick(resetButton, Qt::LeftButton);

    check(approxEqual(gammaCtrl->value(), initialGamma) && approxEqual(contrastCtrl->value(), initialContrast),
          "Reset restores every slider to the value captured when the dialog was opened");

    delete dialog;
}

void testSaveProducesRealPngFileWithFlameDimensions() {
    const char* outputPath = "postprocess_dialog_test_output.png";
    std::remove(outputPath);

    auto flame = makeSierpinskiFlame();
    auto* dialog = new apo::ui::PostProcessDialog(flame);

    auto* saveButton = dialog->findChild<QPushButton*>("saveButton");
    auto* statusLabel = dialog->findChild<QLabel*>("statusLabel");
    auto* progressBar = dialog->findChild<QProgressBar*>("progressBar");
    if (!check(saveButton && statusLabel && progressBar, "saveButton/statusLabel/progressBar found")) {
        delete dialog;
        return;
    }

    // QFileDialog::getSaveFileName() runs its own nested modal event loop -
    // same pattern render_dialog_interaction_test.cpp uses for Browse.
    whenModalShown<QFileDialog>(dialog, [outputPath](QFileDialog* fileDialog) {
        fileDialog->selectFile(outputPath);
        static_cast<QDialog*>(fileDialog)->accept();
    });
    QTest::mouseClick(saveButton, Qt::LeftButton);

    check(!saveButton->isEnabled(), "clicking Save immediately disables Save itself (a render is now in flight)");

    const bool finished = waitUntil([saveButton] { return saveButton->isEnabled(); });
    check(finished, "the full render-and-save completes within the timeout");
    check(progressBar->value() == 100, "the progress bar reaches 100% on a completed save");
    check(statusLabel->text().contains("Saved"), "the status label reports the save completed");

    int outWidth = 0, outHeight = 0;
    std::vector<std::uint8_t> pixels;
    const bool loaded = apo::readPng(outputPath, outWidth, outHeight, pixels);
    check(loaded, "Save produces a real, decodable PNG file at the chosen path");
    check(outWidth == flame->width && outHeight == flame->height,
          "the saved PNG matches the flame's own width/height (PostProcessDialog reuses the flame's existing "
          "size rather than duplicating RenderDialog's size controls)");

    std::remove(outputPath);
    delete dialog;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testControlsStartFromFlameValues();
    testEditingControlsNeverTouchesSharedFlame();
    testResetRestoresSlidersToInitialValues();
    testSaveProducesRealPngFileWithFlameDimensions();

    return apo_test::reportAndExit();
}
