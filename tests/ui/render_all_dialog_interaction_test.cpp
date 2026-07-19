// Real-widget interaction tests for RenderAllDialog (src/ui/
// RenderAllDialog.h) - A3's batch-render counterpart to RenderDialog
// (already covered independently in render_dialog_interaction_test.cpp).
// Verifies: controls seed from the first flame, output filenames are
// sanitized/de-duplicated up front, Render All is gated on an output
// folder, a real click-to-Render-All actually writes one decodable PNG per
// flame (with optional .flame parameter files), Cancel stops the batch
// after the in-flight flame rather than silently continuing, and an empty
// library disables the whole dialog rather than crashing.

#include <cstdio>
#include <functional>

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>
#include <QTimer>

#include "../TestHelpers.h"
#include "RenderAllDialog.h"
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

// Same shape as render_dialog_interaction_test.cpp's makeSierpinskiFlame -
// small and fast, but with real chaos-game structure. `name` lets a batch
// of these be told apart (and exercises the sanitize/de-duplicate path
// when callers pass in colliding or invalid names).
std::shared_ptr<apo::Flame> makeSierpinskiFlame(const std::string& name) {
    auto flame = std::make_shared<apo::Flame>();
    flame->name = name;
    flame->width = 20;
    flame->height = 15;
    flame->sampleDensity = 15;
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

bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 15000) {
    QElapsedTimer timer;
    timer.start();
    while (!pred()) {
        if (timer.elapsed() > timeoutMs) return false;
        QTest::qWait(20);
    }
    return true;
}

// A fresh, empty scratch folder for a test to render into - removed first
// (in case a prior run left it behind) so file-existence checks are never
// fooled by stale output.
QString freshTestFolder(const QString& name) {
    QDir().mkpath(name);
    QDir dir(name);
    for (const QString& entry : dir.entryList(QDir::Files)) dir.remove(entry);
    return QDir::current().filePath(name);
}

void testControlsSeedFromFirstFlame() {
    std::vector<std::shared_ptr<apo::Flame>> flames = {makeSierpinskiFlame("Alpha"), makeSierpinskiFlame("Beta")};
    flames[0]->spatialOversample = 2;
    flames[0]->spatialFilterRadius = 0.6;
    flames[0]->enableDE = true;
    flames[0]->transparency = true;
    auto* dialog = new apo::ui::RenderAllDialog(flames);

    auto* widthSpin = dialog->findChild<QSpinBox*>("widthSpin");
    auto* heightSpin = dialog->findChild<QSpinBox*>("heightSpin");
    auto* densitySpin = dialog->findChild<QDoubleSpinBox*>("densitySpin");
    auto* oversampleSpin = dialog->findChild<QSpinBox*>("oversampleSpin");
    auto* filterRadiusSpin = dialog->findChild<QDoubleSpinBox*>("filterRadiusSpin");
    auto* adaptiveFilterCheck = dialog->findChild<QCheckBox*>("adaptiveFilterCheck");
    auto* transparentBackgroundCheck = dialog->findChild<QCheckBox*>("transparentBackgroundCheck");
    auto* resultsList = dialog->findChild<QListWidget*>("resultsList");
    if (!check(widthSpin && heightSpin && densitySpin && oversampleSpin && filterRadiusSpin && adaptiveFilterCheck &&
                   transparentBackgroundCheck && resultsList,
               "RenderAllDialog exposes all its shared-setting controls and the results list")) {
        delete dialog;
        return;
    }

    check(widthSpin->value() == flames[0]->width, "widthSpin seeds from the first flame's width");
    check(heightSpin->value() == flames[0]->height, "heightSpin seeds from the first flame's height");
    check(approxEqual(densitySpin->value(), flames[0]->sampleDensity), "densitySpin seeds from the first flame's sampleDensity");
    check(oversampleSpin->value() == 2, "oversampleSpin seeds from the first flame's spatialOversample");
    check(approxEqual(filterRadiusSpin->value(), 0.6), "filterRadiusSpin seeds from the first flame's spatialFilterRadius");
    check(adaptiveFilterCheck->isChecked(), "adaptiveFilterCheck seeds from the first flame's enableDE");
    check(transparentBackgroundCheck->isChecked(), "transparentBackgroundCheck seeds from the first flame's transparency");

    check(resultsList->count() == 2, "the results list has one entry per flame");
    check(resultsList->item(0)->text().contains("Alpha") && resultsList->item(0)->text().contains("Alpha.png"),
          "the first result entry names the flame and its planned output file");
    check(resultsList->item(1)->text().contains("Beta") && resultsList->item(1)->text().contains("Beta.png"),
          "the second result entry names the flame and its planned output file");

    delete dialog;
}

void testFileNameSanitizationAndDeduplication() {
    std::vector<std::shared_ptr<apo::Flame>> flames = {
        makeSierpinskiFlame("dup"),
        makeSierpinskiFlame("dup"),
        makeSierpinskiFlame("weird:/name"),
        makeSierpinskiFlame(""),
    };
    auto* dialog = new apo::ui::RenderAllDialog(flames);
    auto* resultsList = dialog->findChild<QListWidget*>("resultsList");
    if (!check(resultsList && resultsList->count() == 4, "resultsList found with 4 entries")) {
        delete dialog;
        return;
    }

    check(resultsList->item(0)->text().contains("dup.png"), "the first 'dup' flame gets the plain sanitized filename");
    check(resultsList->item(1)->text().contains("dup (2).png"),
          "the second 'dup' flame's filename is de-duplicated rather than colliding with the first");
    // The display label keeps the flame's raw (possibly illegal-for-a-path)
    // name verbatim - only the "-> <filename>" half after it needs to be
    // filesystem-legal, so check that half specifically rather than the
    // whole item string.
    const QString illegalNameItemText = resultsList->item(2)->text();
    const QString sanitizedFilePart = illegalNameItemText.mid(illegalNameItemText.indexOf("-> ") + 3);
    check(!sanitizedFilePart.contains(":") && !sanitizedFilePart.contains("/"),
          "filesystem-illegal characters (':' and '/') are stripped from the output filename");
    check(resultsList->item(3)->text().contains("flame.png"),
          "an empty flame name falls back to a non-empty placeholder filename");

    delete dialog;
}

void testEmptyLibraryDisablesRenderAllButton() {
    auto* dialog = new apo::ui::RenderAllDialog({});
    auto* startButton = dialog->findChild<QPushButton*>("startButton");
    auto* overallLabel = dialog->findChild<QLabel*>("overallLabel");
    if (!check(startButton && overallLabel, "startButton and overallLabel found")) {
        delete dialog;
        return;
    }

    check(!startButton->isEnabled(), "Render All stays disabled with an empty flame list, even with a folder chosen");
    check(overallLabel->text().contains("No flames"), "the overall label reports there's nothing to render");

    delete dialog;
}

void testStartButtonDisabledUntilOutputFolderChosen() {
    std::vector<std::shared_ptr<apo::Flame>> flames = {makeSierpinskiFlame("Alpha")};
    auto* dialog = new apo::ui::RenderAllDialog(flames);

    auto* startButton = dialog->findChild<QPushButton*>("startButton");
    auto* outputFolderEdit = dialog->findChild<QLineEdit*>("outputFolderEdit");
    if (!check(startButton && outputFolderEdit, "startButton and outputFolderEdit found")) {
        delete dialog;
        return;
    }

    check(!startButton->isEnabled(), "Render All is disabled with no output folder chosen");
    outputFolderEdit->setText("some_folder");
    check(startButton->isEnabled(), "Render All becomes enabled once an output folder is typed in");
    outputFolderEdit->clear();
    check(!startButton->isEnabled(), "Render All becomes disabled again if the output folder is cleared");

    delete dialog;
}

void testBrowseOpensFolderDialogAndFillsPath() {
    std::vector<std::shared_ptr<apo::Flame>> flames = {makeSierpinskiFlame("Alpha")};
    auto* dialog = new apo::ui::RenderAllDialog(flames);
    auto* browseButton = dialog->findChild<QPushButton*>("browseButton");
    auto* outputFolderEdit = dialog->findChild<QLineEdit*>("outputFolderEdit");
    if (!check(browseButton && outputFolderEdit, "browseButton and outputFolderEdit found")) {
        delete dialog;
        return;
    }

    const QString folder = freshTestFolder("render_all_dialog_browse_test_folder");
    whenModalShown<QFileDialog>(dialog, [folder](QFileDialog* fileDialog) {
        fileDialog->setDirectory(folder);
        static_cast<QDialog*>(fileDialog)->accept();
    });
    QTest::mouseClick(browseButton, Qt::LeftButton);

    check(outputFolderEdit->text().contains("render_all_dialog_browse_test_folder"),
          "Browse... fills the output folder edit with the chosen directory");

    delete dialog;
    QDir(folder).removeRecursively();
}

void testRenderAllRendersEveryFlameToSeparateFiles() {
    const QString folder = freshTestFolder("render_all_dialog_output_test_folder");

    std::vector<std::shared_ptr<apo::Flame>> flames = {makeSierpinskiFlame("Alpha"), makeSierpinskiFlame("Beta")};
    auto* dialog = new apo::ui::RenderAllDialog(flames);

    auto* outputFolderEdit = dialog->findChild<QLineEdit*>("outputFolderEdit");
    auto* startButton = dialog->findChild<QPushButton*>("startButton");
    auto* cancelButton = dialog->findChild<QPushButton*>("cancelButton");
    auto* overallLabel = dialog->findChild<QLabel*>("overallLabel");
    auto* resultsList = dialog->findChild<QListWidget*>("resultsList");
    if (!check(outputFolderEdit && startButton && cancelButton && overallLabel && resultsList, "controls found")) {
        delete dialog;
        return;
    }

    outputFolderEdit->setText(folder);
    QTest::mouseClick(startButton, Qt::LeftButton);
    check(cancelButton->isEnabled(), "clicking Render All immediately enables Cancel");
    check(!startButton->isEnabled(), "clicking Render All immediately disables itself (no re-entrant batches)");

    const bool finished = waitUntil([startButton] { return startButton->isEnabled(); });
    check(finished, "the whole batch completes within the timeout");
    check(overallLabel->text().contains("Done") && overallLabel->text().contains("2 succeeded"),
          "the overall label reports both flames succeeded");

    int w = 0, h = 0;
    std::vector<std::uint8_t> pixels;
    check(apo::readPng((folder + "/Alpha.png").toStdString(), w, h, pixels) && w == 20 && h == 15,
          "Alpha.png is a real, decodable PNG at the dialog's configured width/height");
    check(apo::readPng((folder + "/Beta.png").toStdString(), w, h, pixels) && w == 20 && h == 15,
          "Beta.png is a real, decodable PNG at the dialog's configured width/height");

    check(resultsList->item(0)->text().contains("done") && resultsList->item(1)->text().contains("done"),
          "both results-list entries are annotated as done once the batch finishes");

    delete dialog;
    QDir(folder).removeRecursively();
}

void testSaveParametersWritesAFlameFilePerImage() {
    const QString folder = freshTestFolder("render_all_dialog_save_params_test_folder");

    std::vector<std::shared_ptr<apo::Flame>> flames = {makeSierpinskiFlame("Alpha")};
    auto* dialog = new apo::ui::RenderAllDialog(flames);

    auto* outputFolderEdit = dialog->findChild<QLineEdit*>("outputFolderEdit");
    auto* saveParametersCheck = dialog->findChild<QCheckBox*>("saveParametersCheck");
    auto* startButton = dialog->findChild<QPushButton*>("startButton");
    if (!check(outputFolderEdit && saveParametersCheck && startButton, "controls found")) {
        delete dialog;
        return;
    }
    check(!saveParametersCheck->isChecked(), "Save Parameters starts unchecked");

    outputFolderEdit->setText(folder);
    saveParametersCheck->setChecked(true);
    QTest::mouseClick(startButton, Qt::LeftButton);
    const bool finished = waitUntil([startButton] { return startButton->isEnabled(); });
    if (!check(finished, "the batch completes within the timeout")) {
        delete dialog;
        return;
    }

    auto loaded = apo::loadFlameFile((folder + "/Alpha.flame").toStdString());
    check(!loaded.empty(), "checking Save Parameters writes a loadable .flame file next to each rendered PNG");

    delete dialog;
    QDir(folder).removeRecursively();
}

void testWithoutSaveParametersNoFlameFilesAreWritten() {
    const QString folder = freshTestFolder("render_all_dialog_no_save_params_test_folder");

    std::vector<std::shared_ptr<apo::Flame>> flames = {makeSierpinskiFlame("Alpha")};
    auto* dialog = new apo::ui::RenderAllDialog(flames);

    auto* outputFolderEdit = dialog->findChild<QLineEdit*>("outputFolderEdit");
    auto* startButton = dialog->findChild<QPushButton*>("startButton");
    if (!check(outputFolderEdit && startButton, "controls found")) {
        delete dialog;
        return;
    }

    outputFolderEdit->setText(folder);
    QTest::mouseClick(startButton, Qt::LeftButton);
    const bool finished = waitUntil([startButton] { return startButton->isEnabled(); });
    if (!check(finished, "the batch completes within the timeout")) {
        delete dialog;
        return;
    }

    auto loaded = apo::loadFlameFile((folder + "/Alpha.flame").toStdString());
    check(loaded.empty(), "leaving Save Parameters unchecked (the default) writes no .flame files at all");

    delete dialog;
    QDir(folder).removeRecursively();
}

void testCancelStopsTheBatchAfterTheInFlightFlame() {
    const QString folder = freshTestFolder("render_all_dialog_cancel_test_folder");

    // A larger batch than testRenderAllRendersEveryFlameToSeparateFiles - 5
    // flames, so clicking Cancel immediately after Render All has a real
    // chance of interrupting the batch before every flame gets processed
    // (proving the batch actually stops, not just that it finishes fast
    // enough to race past a cancel).
    std::vector<std::shared_ptr<apo::Flame>> flames;
    for (int i = 0; i < 5; ++i) flames.push_back(makeSierpinskiFlame("Flame" + std::to_string(i)));
    auto* dialog = new apo::ui::RenderAllDialog(flames);

    auto* outputFolderEdit = dialog->findChild<QLineEdit*>("outputFolderEdit");
    auto* startButton = dialog->findChild<QPushButton*>("startButton");
    auto* cancelButton = dialog->findChild<QPushButton*>("cancelButton");
    auto* overallLabel = dialog->findChild<QLabel*>("overallLabel");
    if (!check(outputFolderEdit && startButton && cancelButton && overallLabel, "controls found")) {
        delete dialog;
        return;
    }

    outputFolderEdit->setText(folder);
    QTest::mouseClick(startButton, Qt::LeftButton);
    QTest::mouseClick(cancelButton, Qt::LeftButton);
    check(!cancelButton->isEnabled(), "clicking Cancel immediately disables it, before the worker even responds");

    const bool finished = waitUntil([startButton] { return startButton->isEnabled(); });
    check(finished, "a cancelled batch still reaches a finished state within the timeout");
    check(overallLabel->text().contains("Cancelled"), "the overall label reports the batch was cancelled");
    check(startButton->isEnabled(), "Render All is re-enabled again after a cancelled batch");

    delete dialog;
    QDir(folder).removeRecursively();
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testControlsSeedFromFirstFlame();
    testFileNameSanitizationAndDeduplication();
    testEmptyLibraryDisablesRenderAllButton();
    testStartButtonDisabledUntilOutputFolderChosen();
    testBrowseOpensFolderDialogAndFillsPath();
    testRenderAllRendersEveryFlameToSeparateFiles();
    testSaveParametersWritesAFlameFilePerImage();
    testWithoutSaveParametersNoFlameFilesAreWritten();
    testCancelStopsTheBatchAfterTheInFlightFlame();

    return apo_test::reportAndExit();
}
