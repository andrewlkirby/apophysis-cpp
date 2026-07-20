// Real-widget interaction tests for the Phase 7 "utility dialog batch"
// additions to MainWindow (src/ui/MainWindow.h) and EditorWindow (src/ui/
// EditorWindow.h): New Flame, Save Flame As (from both windows), New
// Random Batch, and the About dialog. FlameIO's load/save round-trip and
// generateRandomFlame() are each already covered independently in
// flame_io_test.cpp and random_flame_test.cpp; this file verifies the Qt
// wiring on top of them - New Flame opens a real, editable blank flame,
// Save Flame As (a real, modal QFileDialog, same testing pattern as
// adjust_dialog_interaction_test.cpp's QColorDialog test) writes a file
// that round-trips through FlameIO, New Random Batch (a real, modal
// QInputDialog) populates the library list with exactly the requested
// count, and About opens/closes cleanly.

#include <cstdio>
#include <fstream>
#include <functional>
#include <memory>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTest>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>

#include "../TestHelpers.h"
#include "AppSettings.h"
#include "EditorWindow.h"
#include "MainWindow.h"
#include "PreviewLabel.h"
#include "RenderAllDialog.h"
#include "SmoothPaletteDialog.h"
#include "core/BuiltinGradients.h"
#include "core/VariationRegistry.h"
#include "core/io/FlameIO.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->name = "editor test flame";
    flame->width = 64;
    flame->height = 64;
    apo::XForm& xf = *flame->xform[0];
    xf.clear();
    xf.density = 1.0;
    xf.c[2] = {0.25, -0.5}; // a distinctive, easy-to-check-after-round-trip value
    return flame;
}

// Polls every 10ms (bounded by timeoutMs) until a modal DialogT is the
// active modal widget, then invokes `onShown` with it once and stops -
// replaces this file's previous pattern of a single QTimer::singleShot(50,
// ...) check, which raced whenever the real dialog took longer than that
// hardcoded 50ms to actually open. That race was real, not theoretical: two
// of this file's own tests (testSaveFlameAsFromMainWindowRoundTrips
// immediately followed by testSaveAllFlamesRoundTripsEveryFlameNotJustSelected)
// reliably hung the whole process when run back to back, in both the normal
// build and a separate AVX2-codegen build investigated for FOLLOWUP_PLAN.txt's
// B7 - confirmed to be this timing race, not AVX2, by reproducing the exact
// same hang in the plain build once isolated down to those two tests. Every
// QTimer::singleShot(50, ...)-based modal-dialog helper in this file (and
// the same pattern in adjust/fullscreen_view/postprocess/render_all/
// render_dialog_interaction_test.cpp) was replaced the same way. `context`
// is the QObject the polling timer is parented to, so it's cleaned up
// automatically if that object is destroyed first (e.g. a test bails out
// early via an early `return` after a failed check()).
// timeoutMs default raised from 5000: a heavily loaded CI/dev-machine
// Docker environment can be slow enough to show a modal dialog that the
// polling timer below only detects after several real seconds - a timeout
// this tight risks a false "the dialog never appeared" result under load,
// not just a hypothetical margin.
template <class DialogT, class F>
void whenModalShown(QObject* context, F onShown, int timeoutMs = 20000) {
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

// Intercepts the next modal QFileDialog::getSaveFileName call and accepts
// it with `path`.
void acceptNextSaveDialogWith(QObject* context, const QString& path) {
    // QFileDialog::selectFile() only special-cases an *absolute* path - a
    // bare filename instead resolves against the dialog's own cached "last
    // visited directory" (QFileDialogPrivate state shared process-wide
    // across every QFileDialog instance created in this binary, not this
    // test's notion of "current directory"). If an earlier test elsewhere
    // in this file already opened a dialog that navigated somewhere else,
    // that cache can drift away from the process's real working directory
    // - and every apo::loadFlameFile(path) call in this file reads
    // relative to the *actual* CWD, not wherever the dialog's cache
    // happened to point the write at. Resolving to an absolute path here
    // removes that ambiguity outright (matches selectFile()'s own
    // documented behavior for absolute paths), independent of whatever
    // state any earlier test left behind.
    const QString absolutePath = QDir(path).isAbsolute() ? path : QDir::current().absoluteFilePath(path);
    whenModalShown<QFileDialog>(context, [absolutePath](QFileDialog* dialog) {
        // QFileDialog::selectFile() proved unreliable in CI (confirmed via
        // diagnostics: dialog->selectedFiles() already reported the
        // dialog's original suggested-name default *immediately* after
        // calling selectFile() - not a race, not a later reset, just a
        // no-op - specifically whenever that suggested default happened to
        // collide with another test's own suggested name elsewhere in this
        // same run, e.g. multiple fixtures named "Alpha"). Bypassing it
        // entirely by driving the dialog's own internal filename QLineEdit
        // directly - "fileNameEdit" is the stable objectName Qt's own
        // QFileDialog implementation has assigned that widget since Qt4,
        // specifically to make exactly this kind of automated testing
        // possible without depending on selectFile()'s own model-matching
        // logic at all.
        auto* fileNameEdit = dialog->findChild<QLineEdit*>("fileNameEdit");
        if (fileNameEdit) {
            fileNameEdit->setText(absolutePath);
        } else {
            dialog->selectFile(absolutePath); // fallback if that internal name ever changes
        }
        static_cast<QDialog*>(dialog)->accept();
    });
}

// Polls `pred` until it's true or `timeoutMs` elapses - same pattern
// triangle_canvas_interaction_test.cpp uses for waiting on an asynchronous
// render to actually complete (RenderWorker runs on its own QThread; a
// render's result only arrives via a queued renderFinished signal, not
// synchronously with whatever triggered it).
bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!pred()) {
        if (timer.elapsed() > timeoutMs) return false;
        QTest::qWait(20);
    }
    return true;
}

struct RandomSettingsGuard {
    int savedMin = apo::ui::AppSettings::randomMinXforms();
    int savedMax = apo::ui::AppSettings::randomMaxXforms();
    int savedSymType = apo::ui::AppSettings::randomSymmetryType();
    int savedSymOrder = apo::ui::AppSettings::randomSymmetryOrder();
    QString savedPrefix = apo::ui::AppSettings::randomBatchTitlePrefix();
    double savedGamma = apo::ui::AppSettings::defaultGamma();
    double savedBrightness = apo::ui::AppSettings::defaultBrightness();
    int savedOversample = apo::ui::AppSettings::defaultOversample();
    int savedGradientSource = apo::ui::AppSettings::randomGradientSource();
    QString savedForcedVariation = apo::ui::AppSettings::randomForcedVariationName();
    QStringList savedDisabledVariations = apo::ui::AppSettings::disabledVariationNames();
    int savedMinVariationsPerXform = apo::ui::AppSettings::randomMinVariationsPerXform();
    int savedMaxVariationsPerXform = apo::ui::AppSettings::randomMaxVariationsPerXform();
    double savedVariationWeightMin = apo::ui::AppSettings::randomVariationWeightMin();
    double savedVariationWeightMax = apo::ui::AppSettings::randomVariationWeightMax();
    bool savedRandomizeVariationParameters = apo::ui::AppSettings::randomizeVariationParameters();
    double savedVariationParameterStrength = apo::ui::AppSettings::randomVariationParameterStrength();

    ~RandomSettingsGuard() {
        apo::ui::AppSettings::setRandomMinXforms(savedMin);
        apo::ui::AppSettings::setRandomMaxXforms(savedMax);
        apo::ui::AppSettings::setRandomSymmetryType(savedSymType);
        apo::ui::AppSettings::setRandomSymmetryOrder(savedSymOrder);
        apo::ui::AppSettings::setRandomBatchTitlePrefix(savedPrefix);
        apo::ui::AppSettings::setDefaultGamma(savedGamma);
        apo::ui::AppSettings::setDefaultBrightness(savedBrightness);
        apo::ui::AppSettings::setDefaultOversample(savedOversample);
        apo::ui::AppSettings::setRandomGradientSource(savedGradientSource);
        apo::ui::AppSettings::setRandomForcedVariationName(savedForcedVariation);
        apo::ui::AppSettings::setDisabledVariationNames(savedDisabledVariations);
        apo::ui::AppSettings::setRandomMinVariationsPerXform(savedMinVariationsPerXform);
        apo::ui::AppSettings::setRandomMaxVariationsPerXform(savedMaxVariationsPerXform);
        apo::ui::AppSettings::setRandomVariationWeightMin(savedVariationWeightMin);
        apo::ui::AppSettings::setRandomVariationWeightMax(savedVariationWeightMax);
        apo::ui::AppSettings::setRandomizeVariationParameters(savedRandomizeVariationParameters);
        apo::ui::AppSettings::setRandomVariationParameterStrength(savedVariationParameterStrength);
    }
};

struct ViewModeSettingsGuard {
    bool savedThumbnailView = apo::ui::AppSettings::libraryThumbnailView();
    ~ViewModeSettingsGuard() { apo::ui::AppSettings::setLibraryThumbnailView(savedThumbnailView); }
};

// Opens `window`'s currently-selected library flame in the Editor and
// saves it to `path` - the only way to inspect a MainWindow-owned Flame's
// actual field values from outside, since neither MainWindow nor
// EditorWindow expose their Flame objects directly. Reuses the same
// modal-QFileDialog-accept pattern as acceptNextSaveDialogWith.
std::vector<std::unique_ptr<apo::Flame>> editSelectedFlameAndSaveTo(apo::ui::MainWindow* window,
                                                                     const std::string& path) {
    auto* editFlameAction = window->findChild<QAction*>("editFlameAction");
    if (!editFlameAction) return {};
    editFlameAction->trigger();

    auto* editor = window->findChild<apo::ui::EditorWindow*>();
    auto* saveFlameAsAction = editor ? editor->findChild<QAction*>("saveFlameAsAction") : nullptr;
    if (!editor || !saveFlameAsAction) return {};

    // EditorWindow's own constructor kicks off its own initial preview
    // render asynchronously, on its own worker QThread, the same way
    // MainWindow's selection-driven preview does (see EditorWindow.cpp's
    // own statusBar()->showMessage("Rendering...")) - triggering the modal
    // Save dialog immediately, while that's still in flight, is a genuine
    // race that reliably hangs the dialog's nested event loop under Linux/
    // Docker's own thread scheduling (verified directly: this was the root
    // cause behind every "Save Flame As ... writes a loadable file" hang/
    // failure this file exercised, not a dialog-implementation issue).
    waitUntil([editor] { return !editor->statusBar()->currentMessage().contains("Rendering"); });

    std::remove(path.c_str());
    acceptNextSaveDialogWith(editor, QString::fromStdString(path));
    saveFlameAsAction->trigger();
    return apo::loadFlameFile(path);
}

// A small, colorful test image - varied colors so a smoothed sample of it
// is meaningfully distinguishable from a flame's own default gradient.
QString writeSmoothPaletteTestImage(const QString& path) {
    QImage image(16, 16, QImage::Format_RGB888);
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            image.setPixelColor(x, y, QColor((x * 37) % 256, (y * 91) % 256, ((x + y) * 53) % 256));
        }
    }
    image.save(path, "PNG");
    return path;
}

std::shared_ptr<apo::Flame> makeNamedFlame(const std::string& name) {
    auto flame = makeTestFlame();
    flame->name = name;
    return flame;
}

// Clicks a standard button (Yes/No/...) on the next modal QMessageBox -
// same modal-dialog-testing pattern as acceptNextSaveDialogWith, for
// onDeleteFlameTriggered's confirmation prompt.
void answerNextMessageBox(QObject* context, QMessageBox::StandardButton button) {
    whenModalShown<QMessageBox>(context, [button](QMessageBox* box) { box->button(button)->click(); });
}

void testNewFlameOpensEditableBlankFlame() {
    auto* window = new apo::ui::MainWindow();
    auto* newFlameAction = window->findChild<QAction*>("newFlameAction");
    if (!check(newFlameAction != nullptr, "MainWindow exposes a newFlameAction")) {
        delete window;
        return;
    }

    newFlameAction->trigger();
    auto* editor = window->findChild<apo::ui::EditorWindow*>();
    if (!check(editor != nullptr, "New Flame opens an EditorWindow child")) {
        delete window;
        return;
    }

    check(editor->windowTitle().contains("Untitled"), "the new flame is titled 'Untitled'");
    auto* xformList = editor->findChild<QListWidget*>();
    if (check(xformList != nullptr, "EditorWindow exposes its xform list")) {
        check(xformList->count() == 1, "a new blank flame starts with exactly one active, editable xform");
    }

    delete window;
}

void testSaveFlameAsFromMainWindowRoundTrips() {
    const std::string inputPath = "main_window_save_flame_test_input.flame";
    const std::string outputPath = "main_window_save_flame_test.flame";
    std::remove(inputPath.c_str());
    std::remove(outputPath.c_str());

    // Self-contained fixture (rather than depending on tests/baselines/'s
    // location relative to the process's working directory, which
    // tests/ui/CMakeLists.txt doesn't pin down the way tests/CMakeLists.txt's
    // APO_BASELINES_DIR does for the plain-core tests).
    auto fixture = makeTestFlame();
    check(apo::saveFlameFile(inputPath, {fixture.get()}), "test setup: writing the input fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));
    auto* saveFlameAsAction = window->findChild<QAction*>("saveFlameAsAction");
    if (!check(saveFlameAsAction != nullptr, "MainWindow exposes a saveFlameAsAction")) {
        delete window;
        return;
    }

    acceptNextSaveDialogWith(window, QString::fromStdString(outputPath));
    saveFlameAsAction->trigger();

    auto loaded = apo::loadFlameFile(outputPath);
    check(!loaded.empty(), "Save Flame As from MainWindow writes a file FlameIO can load back");

    delete window;
    std::remove(inputPath.c_str());
    std::remove(outputPath.c_str());
}

void testSaveAllFlamesRoundTripsEveryFlameNotJustSelected() {
    const std::string inputPath = "main_window_save_all_test_input.flame";
    const std::string outputPath = "main_window_save_all_test_output.flame";
    std::remove(inputPath.c_str());
    std::remove(outputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    auto flameB = makeNamedFlame("Beta");
    auto flameC = makeNamedFlame("Gamma");
    check(apo::saveFlameFile(inputPath, {flameA.get(), flameB.get(), flameC.get()}),
          "test setup: writing a 3-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* saveAllAction = window->findChild<QAction*>("saveAllFlamesAction");
    if (!check(list && saveAllAction, "list and saveAllFlamesAction found")) {
        delete window;
        return;
    }
    if (!check(list->count() == 3, "the fixture's 3 flames are all listed")) {
        delete window;
        return;
    }

    list->setCurrentRow(1); // only "Beta" is selected - Save All must still write all 3

    acceptNextSaveDialogWith(window, QString::fromStdString(outputPath));
    saveAllAction->trigger();

    auto loaded = apo::loadFlameFile(outputPath);
    if (check(loaded.size() == 3, "Save All Parameters writes every flame in the library, not just the selected one")) {
        check(loaded[0]->name == "Alpha" && loaded[1]->name == "Beta" && loaded[2]->name == "Gamma",
              "the saved flames are written in library order with their names intact");
    }

    delete window;
    std::remove(inputPath.c_str());
    std::remove(outputPath.c_str());
}

void testSaveAllFlamesDisabledActionStillPresentWhenLibraryEmpty() {
    // Save All Parameters isn't gated on a *selection* like Edit/Duplicate/
    // Rename/Delete are (there's nothing to select-and-act-on - it always
    // means "the whole library") - only an empty library makes it a no-op,
    // guarded inside onSaveAllFlamesTriggered() itself via an information
    // box rather than disabling the action outright.
    auto* window = new apo::ui::MainWindow();
    auto* saveAllAction = window->findChild<QAction*>("saveAllFlamesAction");
    if (!check(saveAllAction != nullptr, "MainWindow exposes a saveAllFlamesAction")) {
        delete window;
        return;
    }
    check(saveAllAction->isEnabled(), "saveAllFlamesAction is enabled even with nothing selected");

    delete window;
}

void testCopyPutsSelectedFlameXmlOnTheClipboard() {
    const std::string inputPath = "main_window_copy_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    auto flameB = makeNamedFlame("Beta");
    check(apo::saveFlameFile(inputPath, {flameA.get(), flameB.get()}), "test setup: writing a 2-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* copyAction = window->findChild<QAction*>("copyFlameAction");
    if (!check(list && copyAction, "list and copyFlameAction found")) {
        delete window;
        return;
    }

    check(copyAction->isEnabled(), "copyFlameAction is enabled once openFlameFile's automatic row-0 selection lands");

    list->setCurrentRow(1); // "Beta"
    copyAction->trigger();

    const QString clipboardText = QGuiApplication::clipboard()->text();
    check(clipboardText.contains("<flame") && clipboardText.contains("name=\"Beta\""),
          "Copy puts the selected flame's XML (with its own name, not some other flame's) on the system clipboard");

    delete window;
    std::remove(inputPath.c_str());
}

void testPasteInsertsAFreshLibraryEntryFromClipboardXml() {
    const std::string inputPath = "main_window_paste_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    auto flameB = makeNamedFlame("Beta");
    check(apo::saveFlameFile(inputPath, {flameA.get(), flameB.get()}), "test setup: writing a 2-flame fixture succeeds");

    // A distinct fixture, independent of what's already loaded, to paste in -
    // xform[0]->c[2] = {0.25, -0.5} per makeTestFlame(), a value nothing else
    // in this test touches, so recovering it after paste proves the pasted
    // flame's actual fields (not just its name) made it into the library.
    auto toPaste = makeNamedFlame("Pasted");
    const std::string pasteXml = apo::saveFlameToString({toPaste.get()});

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* pasteAction = window->findChild<QAction*>("pasteFlameAction");
    if (!check(list && pasteAction, "list and pasteFlameAction found")) {
        delete window;
        return;
    }
    if (!check(list->count() == 2, "the fixture's 2 flames are both listed")) {
        delete window;
        return;
    }

    list->setCurrentRow(0); // "Alpha" - paste should land right after it, at row 1
    QGuiApplication::clipboard()->setText(QString::fromStdString(pasteXml));
    pasteAction->trigger();

    check(list->count() == 3, "Paste adds exactly one entry to the library");
    if (list->count() == 3) {
        check(list->item(0)->text() == "Alpha" && list->item(1)->text() == "Pasted" && list->item(2)->text() == "Beta",
              "the pasted flame is inserted immediately after the selected entry, matching Duplicate's own "
              "insert-after convention");
        check(list->currentRow() == 1, "Paste selects the newly inserted entry");
    }

    // Selecting the pasted entry (above) kicks off an async preview render
    // on MainWindow's own worker QThread - editSelectedFlameAndSaveTo opens
    // an EditorWindow (which starts rendering too) and immediately drives a
    // modal Save dialog, so let that render settle first. See
    // testUndoRedoCameraGestureSettle's identical comment for why this
    // isn't hypothetical: without this wait, this exact call reliably hangs
    // under Linux/Docker's own thread scheduling.
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    auto loaded = editSelectedFlameAndSaveTo(window, "main_window_paste_fields_test.flame");
    if (check(!loaded.empty(), "the pasted entry's Save Flame As writes a loadable file")) {
        check(approxEqual(loaded.front()->xform[0]->c[2][0], 0.25) &&
                  approxEqual(loaded.front()->xform[0]->c[2][1], -0.5),
              "the pasted flame's actual fields (not just its name) came from the clipboard XML");
    }

    delete window;
    std::remove(inputPath.c_str());
    std::remove("main_window_paste_fields_test.flame");
}

void testPasteIntoEmptyLibraryInsertsAtRowZero() {
    auto* window = new apo::ui::MainWindow();
    auto* list = window->findChild<QListWidget*>();
    auto* pasteAction = window->findChild<QAction*>("pasteFlameAction");
    if (!check(list && pasteAction, "list and pasteFlameAction found")) {
        delete window;
        return;
    }
    check(list->count() == 0, "a fresh MainWindow's library starts empty");

    auto toPaste = makeNamedFlame("Solo");
    QGuiApplication::clipboard()->setText(QString::fromStdString(apo::saveFlameToString({toPaste.get()})));
    pasteAction->trigger();

    check(list->count() == 1 && list->item(0)->text() == "Solo",
          "Paste into an empty library inserts a single entry at row 0, not a crash from selectedIndex_ == -1");

    delete window;
}

void testPasteWithGarbageClipboardShowsAWarningAndChangesNothing() {
    auto* window = new apo::ui::MainWindow();
    auto* list = window->findChild<QListWidget*>();
    auto* pasteAction = window->findChild<QAction*>("pasteFlameAction");
    if (!check(list && pasteAction, "list and pasteFlameAction found")) {
        delete window;
        return;
    }

    QGuiApplication::clipboard()->setText("this is not flame XML");
    answerNextMessageBox(window, QMessageBox::Ok);
    pasteAction->trigger();

    check(list->count() == 0, "pasting unparseable clipboard text leaves the library untouched (a warning box is shown instead)");

    delete window;
}

void testRenderAllFlamesShowsInfoBoxWhenLibraryEmpty() {
    auto* window = new apo::ui::MainWindow();
    auto* renderAllAction = window->findChild<QAction*>("renderAllFlamesAction");
    if (!check(renderAllAction != nullptr, "MainWindow exposes a renderAllFlamesAction")) {
        delete window;
        return;
    }

    bool sawInfoBox = false;
    whenModalShown<QMessageBox>(window, [&sawInfoBox](QMessageBox* box) {
        sawInfoBox = true;
        box->button(QMessageBox::Ok)->click();
    });
    renderAllAction->trigger();

    check(sawInfoBox, "triggering Render All Flames on an empty library shows an info box instead of opening a dialog");
    check(window->findChild<apo::ui::RenderAllDialog*>() == nullptr,
          "no RenderAllDialog is created for an empty library");

    delete window;
}

void testRenderAllFlamesOpensDialogWithTheWholeLibrary() {
    const std::string inputPath = "main_window_render_all_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    auto flameB = makeNamedFlame("Beta");
    check(apo::saveFlameFile(inputPath, {flameA.get(), flameB.get()}), "test setup: writing a 2-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* renderAllAction = window->findChild<QAction*>("renderAllFlamesAction");
    if (!check(renderAllAction != nullptr, "MainWindow exposes a renderAllFlamesAction")) {
        delete window;
        return;
    }

    renderAllAction->trigger();
    auto* dialog = window->findChild<apo::ui::RenderAllDialog*>();
    if (check(dialog != nullptr, "Render All Flames opens a RenderAllDialog child")) {
        auto* resultsList = dialog->findChild<QListWidget*>("resultsList");
        check(resultsList && resultsList->count() == 2,
              "the opened dialog carries every flame currently in the library, not just the selected one");
    }

    delete window;
    std::remove(inputPath.c_str());
}

void testUndoRedoActionsStartDisabledAndHaveStandardShortcuts() {
    auto* window = new apo::ui::MainWindow();
    auto* undoAction = window->findChild<QAction*>("undoAction");
    auto* redoAction = window->findChild<QAction*>("redoAction");
    if (!check(undoAction && redoAction, "MainWindow exposes undoAction/redoAction")) {
        delete window;
        return;
    }

    check(!undoAction->isEnabled() && !redoAction->isEnabled(), "Undo/Redo both start disabled on a fresh window");
    check(undoAction->shortcut() == QKeySequence(QKeySequence::Undo), "Undo uses the platform-standard Undo shortcut");
    check(redoAction->shortcut() == QKeySequence(QKeySequence::Redo), "Redo uses the platform-standard Redo shortcut");

    delete window;
}

void testUndoRedoResetLocation() {
    const std::string inputPath = "main_window_undo_reset_location_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    flameA->zoom = 2.5;
    flameA->center = {37.0, -19.0};
    check(apo::saveFlameFile(inputPath, {flameA.get()}), "test setup: writing a 1-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* resetAction = window->findChild<QAction*>("resetLocationAction");
    auto* undoAction = window->findChild<QAction*>("undoAction");
    auto* redoAction = window->findChild<QAction*>("redoAction");
    if (!check(list && resetAction && undoAction && redoAction, "controls found")) {
        delete window;
        return;
    }
    list->setCurrentRow(0);
    check(!undoAction->isEnabled(), "Undo is still disabled before Reset Location happens");

    resetAction->trigger();
    check(undoAction->isEnabled() && !redoAction->isEnabled(), "Reset Location pushes an undo entry and clears redo");
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });

    undoAction->trigger();
    // Undo/Redo of a Modify entry re-renders the reverted-to state
    // (MainWindow::applyUndoEntry -> startRender) asynchronously on the
    // worker QThread - opening an EditorWindow (editSelectedFlameAndSaveTo,
    // which starts its own render too) while that's still in flight is a
    // real race, not a hypothetical one: it reliably hangs the subsequent
    // modal Save dialog under Linux/Docker's own thread scheduling, even
    // though it happened not to on this project's Windows dev machine.
    // Every other undo/redo trigger below gets the same wait.
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    auto afterUndo = editSelectedFlameAndSaveTo(window, "main_window_undo_reset_location_undo.flame");
    if (check(!afterUndo.empty(), "the flame's Save Flame As writes a loadable file after Undo")) {
        check(approxEqual(afterUndo.front()->zoom, 2.5) && approxEqual(afterUndo.front()->center[0], 37.0) &&
                  approxEqual(afterUndo.front()->center[1], -19.0),
              "Undo restores the exact pre-Reset-Location zoom/center");
    }
    check(!undoAction->isEnabled() && redoAction->isEnabled(), "after undoing the only entry, Undo is disabled and Redo is enabled");

    redoAction->trigger();
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    auto afterRedo = editSelectedFlameAndSaveTo(window, "main_window_undo_reset_location_redo.flame");
    if (check(!afterRedo.empty(), "the flame's Save Flame As writes a loadable file after Redo")) {
        check(approxEqual(afterRedo.front()->zoom, 0.0),
              "Redo re-applies Reset Location's own zoom=0 effect");
        check(!(approxEqual(afterRedo.front()->center[0], 37.0) && approxEqual(afterRedo.front()->center[1], -19.0)),
              "Redo re-applies Reset Location's own auto-framed center, not the stale pre-reset one");
    }
    check(undoAction->isEnabled() && !redoAction->isEnabled(), "after redoing, Undo is enabled again and Redo is disabled");

    delete window;
    std::remove(inputPath.c_str());
    std::remove("main_window_undo_reset_location_undo.flame");
    std::remove("main_window_undo_reset_location_redo.flame");
}

void testUndoRedoRename() {
    const std::string inputPath = "main_window_undo_rename_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    check(apo::saveFlameFile(inputPath, {flameA.get()}), "test setup: writing a 1-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* undoAction = window->findChild<QAction*>("undoAction");
    auto* redoAction = window->findChild<QAction*>("redoAction");
    if (!check(list && undoAction && redoAction, "controls found")) {
        delete window;
        return;
    }

    list->item(0)->setText("Alpha Renamed"); // same programmatic-setText pattern as the existing rename test
    check(list->item(0)->text() == "Alpha Renamed", "the rename took effect in the list");
    check(undoAction->isEnabled(), "renaming pushes an undo entry");

    undoAction->trigger();
    check(list->item(0)->text() == "Alpha", "Undo reverts the list label back to the original name");
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    auto afterUndo = editSelectedFlameAndSaveTo(window, "main_window_undo_rename_undo.flame");
    if (check(!afterUndo.empty(), "Save Flame As after Undo writes a loadable file")) {
        check(afterUndo.front()->name == "Alpha", "Undo reverts the underlying Flame's actual name field too");
    }

    redoAction->trigger();
    check(list->item(0)->text() == "Alpha Renamed", "Redo re-applies the rename to the list label");
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    auto afterRedo = editSelectedFlameAndSaveTo(window, "main_window_undo_rename_redo.flame");
    if (check(!afterRedo.empty(), "Save Flame As after Redo writes a loadable file")) {
        check(afterRedo.front()->name == "Alpha Renamed", "Redo re-applies the rename to the underlying Flame too");
    }

    delete window;
    std::remove(inputPath.c_str());
    std::remove("main_window_undo_rename_undo.flame");
    std::remove("main_window_undo_rename_redo.flame");
}

void testUndoRedoDelete() {
    const std::string inputPath = "main_window_undo_delete_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    auto flameB = makeNamedFlame("Beta");
    auto flameC = makeNamedFlame("Gamma");
    check(apo::saveFlameFile(inputPath, {flameA.get(), flameB.get(), flameC.get()}),
          "test setup: writing a 3-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* deleteAction = window->findChild<QAction*>("deleteFlameAction");
    auto* undoAction = window->findChild<QAction*>("undoAction");
    auto* redoAction = window->findChild<QAction*>("redoAction");
    if (!check(list && deleteAction && undoAction && redoAction, "controls found")) {
        delete window;
        return;
    }

    list->setCurrentRow(1); // "Beta"
    answerNextMessageBox(window, QMessageBox::Yes);
    deleteAction->trigger();
    if (!check(list->count() == 2 && list->item(0)->text() == "Alpha" && list->item(1)->text() == "Gamma",
               "Delete removed exactly 'Beta', leaving Alpha/Gamma")) {
        delete window;
        return;
    }
    check(undoAction->isEnabled(), "Delete pushes an undo entry");

    undoAction->trigger();
    check(list->count() == 3, "Undo restores the deleted entry, bringing the count back to 3");
    if (list->count() == 3) {
        check(list->item(0)->text() == "Alpha" && list->item(1)->text() == "Beta" && list->item(2)->text() == "Gamma",
              "Undo restores 'Beta' at its original index, not just anywhere in the list");
    }
    check(redoAction->isEnabled(), "after undoing a Delete, Redo becomes available");

    redoAction->trigger();
    check(list->count() == 2, "Redo removes 'Beta' again, back to a count of 2");
    if (list->count() == 2) {
        check(list->item(0)->text() == "Alpha" && list->item(1)->text() == "Gamma",
              "Redo removes exactly the same entry Delete originally removed");
    }

    delete window;
    std::remove(inputPath.c_str());
}

void testUndoRedoPaste() {
    const std::string inputPath = "main_window_undo_paste_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    auto flameB = makeNamedFlame("Beta");
    check(apo::saveFlameFile(inputPath, {flameA.get(), flameB.get()}), "test setup: writing a 2-flame fixture succeeds");

    auto toPaste = makeNamedFlame("Pasted");
    const std::string pasteXml = apo::saveFlameToString({toPaste.get()});

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* pasteAction = window->findChild<QAction*>("pasteFlameAction");
    auto* undoAction = window->findChild<QAction*>("undoAction");
    auto* redoAction = window->findChild<QAction*>("redoAction");
    if (!check(list && pasteAction && undoAction && redoAction, "controls found")) {
        delete window;
        return;
    }

    list->setCurrentRow(0); // "Alpha" - Paste lands right after it, at row 1
    QGuiApplication::clipboard()->setText(QString::fromStdString(pasteXml));
    pasteAction->trigger();
    if (!check(list->count() == 3 && list->item(1)->text() == "Pasted", "Paste inserted 'Pasted' at row 1")) {
        delete window;
        return;
    }
    check(undoAction->isEnabled(), "Paste pushes an undo entry");

    undoAction->trigger();
    check(list->count() == 2 && list->item(0)->text() == "Alpha" && list->item(1)->text() == "Beta",
          "Undo removes the pasted entry, restoring the original 2-flame library");
    check(redoAction->isEnabled(), "after undoing a Paste, Redo becomes available");

    redoAction->trigger();
    check(list->count() == 3 && list->item(1)->text() == "Pasted", "Redo re-inserts the pasted entry at the same row");
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    auto afterRedo = editSelectedFlameAndSaveTo(window, "main_window_undo_paste_redo.flame");
    if (check(!afterRedo.empty(), "Save Flame As after Redo writes a loadable file")) {
        check(afterRedo.front()->name == "Pasted", "Redo's re-inserted entry is selected and carries the original pasted content");
    }

    delete window;
    std::remove(inputPath.c_str());
    std::remove("main_window_undo_paste_redo.flame");
}

void testUndoRedoCameraGestureSettle() {
    const std::string inputPath = "main_window_undo_camera_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    check(apo::saveFlameFile(inputPath, {flameA.get()}), "test setup: writing a 1-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* previewLabel = window->findChild<apo::ui::PreviewLabel*>();
    auto* undoAction = window->findChild<QAction*>("undoAction");
    auto* redoAction = window->findChild<QAction*>("redoAction");
    if (!check(previewLabel && undoAction && redoAction, "controls found")) {
        delete window;
        return;
    }

    const bool initialRenderSettled =
        waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    if (!check(initialRenderSettled, "the initial preview render completes")) {
        delete window;
        return;
    }
    check(!undoAction->isEnabled(), "Undo is still disabled before any camera gesture happens");

    QWheelEvent wheel(QPointF(50, 50), previewLabel->mapToGlobal(QPoint(50, 50)), QPoint(0, 0), QPoint(0, 120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(previewLabel, &wheel);
    check(undoAction->isEnabled(), "a settled wheel-zoom gesture pushes exactly one undo entry");
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });

    undoAction->trigger();
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    auto afterUndo = editSelectedFlameAndSaveTo(window, "main_window_undo_camera_undo.flame");
    if (check(!afterUndo.empty(), "Save Flame As after Undo writes a loadable file")) {
        check(approxEqual(afterUndo.front()->zoom, 0.0), "Undo reverts the wheel-zoom gesture's Flame::zoom change back to 0");
    }
    check(redoAction->isEnabled(), "after undoing the camera gesture, Redo becomes available");

    redoAction->trigger();
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    auto afterRedo = editSelectedFlameAndSaveTo(window, "main_window_undo_camera_redo.flame");
    if (check(!afterRedo.empty(), "Save Flame As after Redo writes a loadable file")) {
        check(!approxEqual(afterRedo.front()->zoom, 0.0), "Redo re-applies the wheel-zoom gesture's Flame::zoom change");
    }

    delete window;
    std::remove(inputPath.c_str());
    std::remove("main_window_undo_camera_undo.flame");
    std::remove("main_window_undo_camera_redo.flame");
}

void testUndoStackIsBoundedTo32Entries() {
    const std::string inputPath = "main_window_undo_bound_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Start");
    check(apo::saveFlameFile(inputPath, {flameA.get()}), "test setup: writing a 1-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* undoAction = window->findChild<QAction*>("undoAction");
    if (!check(list && undoAction, "controls found")) {
        delete window;
        return;
    }

    // 35 renames "R1".."R35" - more than kMaxUndoEntries (32) worth of
    // Modify entries, so the oldest 3 (R0->R1, R1->R2, R2->R3) must have
    // been trimmed off the front of undoStack_ by the time this loop ends.
    for (int i = 1; i <= 35; ++i) list->item(0)->setText(QString("R%1").arg(i));
    check(list->item(0)->text() == "R35", "all 35 renames took effect");

    // Undoing exactly 32 times should walk back to "R3" (the oldest
    // surviving entry's "before" value) - a 33rd undo must then be a
    // silent no-op, proving the bound is real and not just coincidentally
    // wide enough.
    for (int i = 0; i < 32; ++i) undoAction->trigger();
    check(list->item(0)->text() == "R3",
          "after exactly 32 undos, the name lands on 'R3' - the boundary kMaxUndoEntries=32 promises, not further back");
    check(!undoAction->isEnabled(), "the undo stack is now empty (older history beyond the 32-entry bound was discarded)");

    undoAction->trigger(); // no-op - nothing left to undo
    check(list->item(0)->text() == "R3", "a 33rd undo attempt past an empty stack changes nothing");

    delete window;
    std::remove(inputPath.c_str());
}

void testSmoothPaletteActionGatedOnSelection() {
    auto* window = new apo::ui::MainWindow();
    auto* smoothPaletteAction = window->findChild<QAction*>("smoothPaletteAction");
    if (!check(smoothPaletteAction != nullptr, "MainWindow exposes a smoothPaletteAction")) {
        delete window;
        return;
    }
    check(!smoothPaletteAction->isEnabled(),
          "smoothPaletteAction starts disabled - a fresh MainWindow has nothing selected");

    delete window;
}

void testSmoothPaletteAppliesToSelectedFlameWithUndo() {
    const std::string inputPath = "main_window_smooth_palette_test_input.flame";
    std::remove(inputPath.c_str());
    const QString imagePath = writeSmoothPaletteTestImage("main_window_smooth_palette_test_image.png");

    auto flameA = makeNamedFlame("Alpha");
    check(apo::saveFlameFile(inputPath, {flameA.get()}), "test setup: writing a 1-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* smoothPaletteAction = window->findChild<QAction*>("smoothPaletteAction");
    auto* undoAction = window->findChild<QAction*>("undoAction");
    auto* redoAction = window->findChild<QAction*>("redoAction");
    if (!check(list && smoothPaletteAction && undoAction && redoAction, "controls found")) {
        delete window;
        return;
    }
    list->setCurrentRow(0);
    check(smoothPaletteAction->isEnabled(), "smoothPaletteAction becomes enabled once a library entry is selected");
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });

    // The reference to compare against after Undo: the flame's cmap as it
    // actually exists in the window right now, not flameA's own in-memory
    // value - loadFlameFile's own palette parser always fills alpha=255
    // (see FlameIO.cpp's parsePalette), which differs from a freshly
    // default-constructed Flame's cmap (alpha=0), so the *loaded* flame's
    // cmap is the only correct "before" baseline here.
    auto beforeApply = editSelectedFlameAndSaveTo(window, "main_window_smooth_palette_before_apply.flame");
    if (!check(!beforeApply.empty(), "Save Flame As before Apply writes a loadable file")) {
        delete window;
        return;
    }
    const apo::ColorMap originalCmap = beforeApply.front()->cmap;

    smoothPaletteAction->trigger();
    auto* dialog = window->findChild<apo::ui::SmoothPaletteDialog*>();
    if (!check(dialog != nullptr, "triggering Smooth Palette opens a SmoothPaletteDialog child")) {
        delete window;
        return;
    }

    auto* applyButton = dialog->findChild<QPushButton*>("applyButton");
    if (!check(applyButton != nullptr, "applyButton found")) {
        delete window;
        return;
    }
    dialog->openImageFile(imagePath);
    check(!undoAction->isEnabled(),
          "opening the dialog and generating a preview doesn't itself push an undo entry - only Apply does");

    QTest::mouseClick(applyButton, Qt::LeftButton);
    check(undoAction->isEnabled(), "clicking Apply pushes an undo entry");
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });

    auto afterApply = editSelectedFlameAndSaveTo(window, "main_window_smooth_palette_after_apply.flame");
    apo::ColorMap appliedCmap{};
    bool cmapChanged = false;
    if (check(!afterApply.empty(), "Save Flame As after Apply writes a loadable file")) {
        appliedCmap = afterApply.front()->cmap;
        cmapChanged = !(appliedCmap.entries == originalCmap.entries);
        check(cmapChanged, "Apply actually changed the selected flame's gradient");
    }

    undoAction->trigger();
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    auto afterUndo = editSelectedFlameAndSaveTo(window, "main_window_smooth_palette_after_undo.flame");
    if (check(!afterUndo.empty(), "Save Flame As after Undo writes a loadable file")) {
        check(afterUndo.front()->cmap.entries == originalCmap.entries, "Undo restores the flame's original gradient");
    }
    check(redoAction->isEnabled(), "after undoing Smooth Palette, Redo becomes available");

    redoAction->trigger();
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    auto afterRedo = editSelectedFlameAndSaveTo(window, "main_window_smooth_palette_after_redo.flame");
    if (check(!afterRedo.empty() && cmapChanged, "Save Flame As after Redo writes a loadable file")) {
        check(afterRedo.front()->cmap.entries == appliedCmap.entries,
              "Redo re-applies the exact same smoothed gradient Apply originally produced");
    }

    delete window;
    std::remove(inputPath.c_str());
    std::remove(imagePath.toStdString().c_str());
    std::remove("main_window_smooth_palette_before_apply.flame");
    std::remove("main_window_smooth_palette_after_apply.flame");
    std::remove("main_window_smooth_palette_after_undo.flame");
    std::remove("main_window_smooth_palette_after_redo.flame");
}

// A7 - drag-and-drop. Qt has no synthetic "perform an OS drag" helper (real
// drag-and-drop involves the window manager), so these construct real
// QDragEnterEvent/QDropEvent objects directly and dispatch them via
// QApplication::sendEvent - same "construct the real event type, send it
// straight to the widget" approach the wheel-event camera-gesture tests
// above already use for QWheelEvent. A DragEnter must be sent (and
// accepted) before the Drop, even though MainWindow's own dropEvent()
// doesn't itself depend on dragEnterEvent() having run first - confirmed
// directly (not assumed): sending a bare QDropEvent with no preceding
// QDragEnterEvent to a QMainWindow never reaches the subclass's own
// dropEvent() override at all, apparently swallowed by QMainWindow's own
// internal toolbar/dock-widget drag-session tracking. A real OS drag
// always sends DragEnter before Drop anyway, so this only affects how
// these synthetic tests must be built, not anything about the feature
// itself.
void simulateFileDrop(apo::ui::MainWindow* window, const QMimeData& mime) {
    QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window, &enter);
    QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window, &drop);
}

void testDroppingAFlameFileOpensItLikeFileOpen() {
    const std::string inputPath = "main_window_drop_test_input.flame";
    std::remove(inputPath.c_str());
    auto flame = makeNamedFlame("Dropped");
    check(apo::saveFlameFile(inputPath, {flame.get()}), "test setup: writing a 1-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    auto* list = window->findChild<QListWidget*>();
    if (!check(list != nullptr, "list found")) {
        delete window;
        return;
    }
    check(list->count() == 0, "a fresh window's library starts empty");

    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QString::fromStdString(inputPath))});
    simulateFileDrop(window, mime);

    check(list->count() == 1 && list->item(0)->text() == "Dropped",
          "dropping a .flame file's URL onto the window opens it into the library, same as File > Open");

    delete window;
    std::remove(inputPath.c_str());
}

void testDragEnterAcceptsLocalFileUrlsAndRejectsPlainText() {
    auto* window = new apo::ui::MainWindow();

    QMimeData fileMime;
    fileMime.setUrls({QUrl::fromLocalFile("some_file.flame")});
    QDragEnterEvent acceptEvent(QPoint(10, 10), Qt::CopyAction, &fileMime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window, &acceptEvent);
    check(acceptEvent.isAccepted(), "dragging a local file URL over the window is accepted");

    QMimeData textMime;
    textMime.setText("not a file");
    QDragEnterEvent rejectEvent(QPoint(10, 10), Qt::CopyAction, &textMime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window, &rejectEvent);
    check(!rejectEvent.isAccepted(), "dragging plain text with no file URLs is rejected, not accepted indiscriminately");

    delete window;
}

void testDroppingAMultiFlameFileListsAllOfThem() {
    const std::string inputPath = "main_window_drop_multi_test_input.flame";
    std::remove(inputPath.c_str());
    auto flameA = makeNamedFlame("Alpha");
    auto flameB = makeNamedFlame("Beta");
    check(apo::saveFlameFile(inputPath, {flameA.get(), flameB.get()}), "test setup: writing a 2-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    auto* list = window->findChild<QListWidget*>();
    if (!check(list != nullptr, "list found")) {
        delete window;
        return;
    }

    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QString::fromStdString(inputPath))});
    simulateFileDrop(window, mime);

    check(list->count() == 2 && list->item(0)->text() == "Alpha" && list->item(1)->text() == "Beta",
          "dropping a file with multiple <flame> entries lists all of them, exactly like openFlameFile()");

    delete window;
    std::remove(inputPath.c_str());
}

void testDroppingAnUnparseableFileShowsAWarningAndChangesNothing() {
    const std::string badPath = "main_window_drop_bad_test.flame";
    {
        std::ofstream out(badPath);
        out << "this is not xml at all { }";
    }

    auto* window = new apo::ui::MainWindow();
    auto* list = window->findChild<QListWidget*>();
    if (!check(list != nullptr, "list found")) {
        delete window;
        return;
    }

    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QString::fromStdString(badPath))});
    answerNextMessageBox(window, QMessageBox::Ok);
    simulateFileDrop(window, mime);

    check(list->count() == 0, "dropping an unparseable file leaves the library empty (a warning box is shown instead)");

    delete window;
    std::remove(badPath.c_str());
}

void testSaveFlameAsFromEditorWindowRoundTripsEdits() {
    const std::string path = "editor_window_save_flame_test.flame";
    std::remove(path.c_str());

    auto flame = makeTestFlame();
    auto* editor = new apo::ui::EditorWindow(flame);
    auto* saveFlameAsAction = editor->findChild<QAction*>("saveFlameAsAction");
    if (!check(saveFlameAsAction != nullptr, "EditorWindow exposes a saveFlameAsAction")) {
        delete editor;
        return;
    }

    // EditorWindow's constructor already kicked off its own initial preview
    // render asynchronously - see editSelectedFlameAndSaveTo's own comment
    // on why racing the modal Save dialog against it reliably hangs under
    // Linux/Docker's own thread scheduling.
    waitUntil([editor] { return !editor->statusBar()->currentMessage().contains("Rendering"); });

    acceptNextSaveDialogWith(editor, QString::fromStdString(path));
    saveFlameAsAction->trigger();

    auto loaded = apo::loadFlameFile(path);
    if (check(!loaded.empty(), "Save Flame As from EditorWindow writes a file FlameIO can load back")) {
        check(approxEqual(loaded.front()->xform[0]->c[2][0], 0.25) &&
                  approxEqual(loaded.front()->xform[0]->c[2][1], -0.5),
              "the saved file reflects the actual edited flame's coefficients, not some default");
    }

    delete editor;
    std::remove(path.c_str());
}

void testNewRandomBatchGeneratesFramedFlames() {
    auto* window = new apo::ui::MainWindow();
    auto* action = window->findChild<QAction*>("newRandomBatchAction");
    if (!check(action != nullptr, "MainWindow exposes a newRandomBatchAction")) {
        delete window;
        return;
    }

    // QInputDialog::getInt() is a blocking modal call too - same
    // whenModalShown pattern as the Save Flame As tests above, just against
    // QInputDialog instead of QFileDialog.
    whenModalShown<QInputDialog>(window, [](QInputDialog* dialog) {
        dialog->setIntValue(3);
        dialog->accept();
    });
    action->trigger();

    auto* list = window->findChild<QListWidget*>();
    if (check(list != nullptr, "MainWindow exposes its flame list")) {
        check(list->count() == 3, "New Random Batch generates exactly the requested number of flames");
    }
    check(window->windowTitle().contains("Random Batch"), "the window title reflects the random batch");

    delete window;
}

void testNewRandomBatchHasCtrlBShortcut() {
    auto* window = new apo::ui::MainWindow();
    auto* action = window->findChild<QAction*>("newRandomBatchAction");
    if (!check(action != nullptr, "MainWindow exposes a newRandomBatchAction")) {
        delete window;
        return;
    }
    check(action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_B),
          "New Random Batch has a Ctrl+B shortcut, matching Main.dfm's own mnuRandomBatch (ShortCut = 16450, "
          "decoded as Ctrl+B)");

    delete window;
}

void testEditFlameActionOpensEditorForARandomBatchFlame() {
    auto* window = new apo::ui::MainWindow();
    auto* batchAction = window->findChild<QAction*>("newRandomBatchAction");
    auto* editAction = window->findChild<QAction*>("editFlameAction");
    auto* list = window->findChild<QListWidget*>();
    if (!check(batchAction && editAction && list, "newRandomBatchAction/editFlameAction/list found")) {
        delete window;
        return;
    }
    check(!editAction->isEnabled(), "editFlameAction starts disabled - a fresh MainWindow's library is empty");

    whenModalShown<QInputDialog>(window, [](QInputDialog* dialog) {
        dialog->setIntValue(2);
        dialog->accept();
    });
    batchAction->trigger();

    if (!check(list->count() == 2, "the random batch populated the library with 2 flames")) {
        delete window;
        return;
    }
    check(editAction->isEnabled(), "editFlameAction becomes enabled once a batch-generated flame is selected");

    editAction->trigger();
    auto* editor = window->findChild<apo::ui::EditorWindow*>();
    if (check(editor != nullptr, "Edit Flame opens an EditorWindow for a random-batch-generated flame")) {
        check(editor->windowTitle().contains("Random 1"),
              "the opened editor is for the actually-selected batch flame (the first one), not some other one");
    }

    delete window;
}

void testLibraryActionsStartDisabledUntilSelection() {
    auto* window = new apo::ui::MainWindow();
    auto* editAction = window->findChild<QAction*>("editFlameAction");
    auto* duplicateAction = window->findChild<QAction*>("duplicateFlameAction");
    auto* renameAction = window->findChild<QAction*>("renameFlameAction");
    auto* deleteAction = window->findChild<QAction*>("deleteFlameAction");
    auto* resetLocationAction = window->findChild<QAction*>("resetLocationAction");
    if (!check(editAction && duplicateAction && renameAction && deleteAction && resetLocationAction,
               "editFlameAction/duplicateFlameAction/renameFlameAction/deleteFlameAction/resetLocationAction "
               "found")) {
        delete window;
        return;
    }

    check(!editAction->isEnabled() && !duplicateAction->isEnabled() && !renameAction->isEnabled() &&
              !deleteAction->isEnabled() && !resetLocationAction->isEnabled(),
          "Edit/Duplicate/Rename/Delete/Reset Location all start disabled - a fresh MainWindow has nothing "
          "selected in its (empty) library");

    delete window;
}

void testRenameCommitsToTheUnderlyingFlameOnly() {
    const std::string inputPath = "main_window_rename_test_input.flame";
    const std::string outputPath = "main_window_rename_test_output.flame";
    std::remove(inputPath.c_str());
    std::remove(outputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    auto flameB = makeNamedFlame("Beta");
    check(apo::saveFlameFile(inputPath, {flameA.get(), flameB.get()}), "test setup: writing a 2-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* renameAction = window->findChild<QAction*>("renameFlameAction");
    auto* saveFlameAsAction = window->findChild<QAction*>("saveFlameAsAction");
    if (!check(list && renameAction && saveFlameAsAction, "list/renameFlameAction/saveFlameAsAction found")) {
        delete window;
        return;
    }
    if (!check(list->count() == 2, "the fixture's 2 flames are both listed")) {
        delete window;
        return;
    }

    list->setCurrentRow(1); // "Beta"
    check(renameAction->isEnabled(), "renameFlameAction becomes enabled once a library entry is selected");

    // Programmatic setText() goes through the same setData()/itemChanged
    // path a user finishing an in-place edit would - a robust way to
    // exercise onFlameItemChanged() without driving the actual inline
    // editor widget on the offscreen QPA platform.
    list->item(1)->setText("Beta Renamed");

    acceptNextSaveDialogWith(window, QString::fromStdString(outputPath));
    saveFlameAsAction->trigger();

    auto loaded = apo::loadFlameFile(outputPath);
    if (check(!loaded.empty(), "Save Flame As after a rename still writes a loadable file")) {
        check(loaded.front()->name == "Beta Renamed",
              "the rename reached the underlying Flame's name field, not just the list label");
    }

    delete window;
    std::remove(inputPath.c_str());
    std::remove(outputPath.c_str());
}

void testDeleteRemovesEntryFromLibraryWithConfirmation() {
    const std::string inputPath = "main_window_delete_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    auto flameB = makeNamedFlame("Beta");
    auto flameC = makeNamedFlame("Gamma");
    check(apo::saveFlameFile(inputPath, {flameA.get(), flameB.get(), flameC.get()}),
          "test setup: writing a 3-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* deleteAction = window->findChild<QAction*>("deleteFlameAction");
    if (!check(list && deleteAction, "list and deleteFlameAction found")) {
        delete window;
        return;
    }
    if (!check(list->count() == 3, "the fixture's 3 flames are all listed")) {
        delete window;
        return;
    }

    list->setCurrentRow(1); // "Beta"
    answerNextMessageBox(window, QMessageBox::Yes);
    deleteAction->trigger();

    check(list->count() == 2, "confirming Delete removes exactly one entry from the library list");
    if (list->count() == 2) {
        check(list->item(0)->text() == "Alpha" && list->item(1)->text() == "Gamma",
              "the remaining entries are exactly the ones not deleted, in their original order");
    }

    delete window;
    std::remove(inputPath.c_str());
}

void testDeleteCancelledLeavesLibraryUnchanged() {
    const std::string inputPath = "main_window_delete_cancel_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    auto flameB = makeNamedFlame("Beta");
    check(apo::saveFlameFile(inputPath, {flameA.get(), flameB.get()}), "test setup: writing a 2-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* deleteAction = window->findChild<QAction*>("deleteFlameAction");
    if (!check(list && deleteAction, "list and deleteFlameAction found")) {
        delete window;
        return;
    }

    list->setCurrentRow(0);
    answerNextMessageBox(window, QMessageBox::No);
    deleteAction->trigger();

    check(list->count() == 2, "declining the confirmation prompt leaves the library untouched");

    delete window;
    std::remove(inputPath.c_str());
}

void testDuplicateFlameInsertsCopyAfterOriginalAndSelectsIt() {
    const std::string inputPath = "main_window_duplicate_test_input.flame";
    std::remove(inputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    auto flameB = makeNamedFlame("Beta");
    auto flameC = makeNamedFlame("Gamma");
    check(apo::saveFlameFile(inputPath, {flameA.get(), flameB.get(), flameC.get()}),
          "test setup: writing a 3-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* duplicateAction = window->findChild<QAction*>("duplicateFlameAction");
    if (!check(list && duplicateAction, "list and duplicateFlameAction found")) {
        delete window;
        return;
    }
    if (!check(list->count() == 3, "the fixture's 3 flames are all listed")) {
        delete window;
        return;
    }

    list->setCurrentRow(1); // "Beta"
    check(duplicateAction->isEnabled(), "duplicateFlameAction becomes enabled once a library entry is selected");
    duplicateAction->trigger();

    check(list->count() == 4, "Duplicate Flame adds exactly one entry to the library");
    if (list->count() == 4) {
        check(list->item(0)->text() == "Alpha" && list->item(1)->text() == "Beta",
              "entries before the duplicated one are untouched");
        check(list->item(2)->text() == "Beta copy", "the duplicate is inserted immediately after its source");
        check(list->item(3)->text() == "Gamma",
              "entries after the duplicated one shift down a row but keep their own identity");
        check(list->currentRow() == 2, "Duplicate Flame selects the newly created copy");
    }

    delete window;
    std::remove(inputPath.c_str());
}

void testDuplicateFlameCopiesEveryFieldViaClone() {
    const std::string inputPath = "main_window_duplicate_fields_test_input.flame";
    const std::string outputPath = "main_window_duplicate_fields_test_output.flame";
    std::remove(inputPath.c_str());
    std::remove(outputPath.c_str());

    auto flameA = makeNamedFlame("Alpha"); // xform[0]->c[2] = {0.25, -0.5} - see makeTestFlame()
    check(apo::saveFlameFile(inputPath, {flameA.get()}), "test setup: writing a 1-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* duplicateAction = window->findChild<QAction*>("duplicateFlameAction");
    if (!check(list && duplicateAction, "list and duplicateFlameAction found")) {
        delete window;
        return;
    }

    list->setCurrentRow(0);
    duplicateAction->trigger();
    if (!check(list->count() == 2 && list->currentRow() == 1, "the duplicate is created and selected")) {
        delete window;
        return;
    }
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });

    auto loaded = editSelectedFlameAndSaveTo(window, outputPath);
    if (check(!loaded.empty(), "the duplicate's Save Flame As writes a loadable file")) {
        check(approxEqual(loaded.front()->xform[0]->c[2][0], 0.25) &&
                  approxEqual(loaded.front()->xform[0]->c[2][1], -0.5),
              "the duplicate is a full clone - its own affine coefficients match the source's exactly");
    }

    delete window;
    std::remove(inputPath.c_str());
    std::remove(outputPath.c_str());
}

void testResetLocationRecomputesZoomAndCenter() {
    const std::string inputPath = "main_window_reset_location_test_input.flame";
    const std::string outputPath = "main_window_reset_location_test_output.flame";
    std::remove(inputPath.c_str());
    std::remove(outputPath.c_str());

    auto flameA = makeNamedFlame("Alpha");
    flameA->zoom = 2.5;
    flameA->center = {37.0, -19.0}; // simulates a flame already zoomed/panned away from its own content
    check(apo::saveFlameFile(inputPath, {flameA.get()}), "test setup: writing a 1-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(inputPath));

    auto* list = window->findChild<QListWidget*>();
    auto* resetAction = window->findChild<QAction*>("resetLocationAction");
    if (!check(list && resetAction, "list and resetLocationAction found")) {
        delete window;
        return;
    }
    list->setCurrentRow(0);
    check(resetAction->isEnabled(), "resetLocationAction becomes enabled once a library entry is selected");
    resetAction->trigger();
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });

    auto loaded = editSelectedFlameAndSaveTo(window, outputPath);
    if (check(!loaded.empty(), "Save Flame As after Reset Location still writes a loadable file")) {
        check(approxEqual(loaded.front()->zoom, 0.0), "Reset Location zeroes the flame's zoom");
        check(!(approxEqual(loaded.front()->center[0], 37.0) && approxEqual(loaded.front()->center[1], -19.0)),
              "Reset Location recomputes center away from its stale, pre-reset value (auto-framed onto the "
              "flame's own actual content, not just zeroed)");
    }

    delete window;
    std::remove(inputPath.c_str());
    std::remove(outputPath.c_str());
}

void testResetLocationHasF12Shortcut() {
    auto* window = new apo::ui::MainWindow();
    auto* action = window->findChild<QAction*>("resetLocationAction");
    if (!check(action != nullptr, "MainWindow exposes a resetLocationAction")) {
        delete window;
        return;
    }
    check(action->shortcut() == QKeySequence(Qt::Key_F12),
          "Reset Location has an F12 shortcut, matching Main.dfm's own mnuResetLocation (ShortCut = 123 = F12, no "
          "modifiers)");
    delete window;
}

void testViewModeDefaultsToThumbnailsAndTogglesTheListWidgetViewMode() {
    ViewModeSettingsGuard guard;
    apo::ui::AppSettings::setLibraryThumbnailView(true);

    auto* window = new apo::ui::MainWindow();
    auto* list = window->findChild<QListWidget*>();
    auto* viewThumbnailsAction = window->findChild<QAction*>("viewThumbnailsAction");
    auto* viewListAction = window->findChild<QAction*>("viewListAction");
    if (!check(list && viewThumbnailsAction && viewListAction,
               "list/viewThumbnailsAction/viewListAction found")) {
        delete window;
        return;
    }

    check(viewThumbnailsAction->isChecked() && !viewListAction->isChecked(),
          "Thumbnails starts checked, matching this port's own established default (see AppSettings.h)");
    check(list->viewMode() == QListView::IconMode, "the list widget starts in IconMode");

    viewListAction->trigger();
    check(!viewThumbnailsAction->isChecked() && viewListAction->isChecked(),
          "triggering List switches the mutually-exclusive toolbar pair's checked state");
    check(list->viewMode() == QListView::ListMode, "triggering List switches the list widget to ListMode");
    check(!apo::ui::AppSettings::libraryThumbnailView(), "triggering List persists the choice to AppSettings");

    viewThumbnailsAction->trigger();
    check(list->viewMode() == QListView::IconMode, "triggering Thumbnails switches back to IconMode");
    check(apo::ui::AppSettings::libraryThumbnailView(), "triggering Thumbnails persists the choice to AppSettings");

    delete window;
}

void testListViewSkipsThumbnailGenerationForNewEntries() {
    ViewModeSettingsGuard guard;
    apo::ui::AppSettings::setLibraryThumbnailView(false); // list mode active *before* the library is populated

    const std::string path = "main_window_view_mode_no_thumb_test.flame";
    std::remove(path.c_str());
    auto flame = makeNamedFlame("Alpha");
    check(apo::saveFlameFile(path, {flame.get()}), "test setup: writing a 1-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(path));

    auto* list = window->findChild<QListWidget*>();
    if (!check(list && list->count() == 1, "the fixture flame is listed")) {
        delete window;
        return;
    }

    // Give a (hoped-to-be-absent) ThumbnailTask a real chance to complete -
    // these are tiny fixture flames, QThreadPool finishes them well under
    // this window if one was actually requested.
    QTest::qWait(300);
    check(list->item(0)->icon().isNull(), "list mode never requests a thumbnail for a newly listed flame");

    delete window;
    std::remove(path.c_str());
}

void testSwitchingToThumbnailViewBackfillsExistingEntries() {
    ViewModeSettingsGuard guard;
    apo::ui::AppSettings::setLibraryThumbnailView(false);

    const std::string path = "main_window_view_mode_backfill_test.flame";
    std::remove(path.c_str());
    auto flame = makeNamedFlame("Alpha");
    check(apo::saveFlameFile(path, {flame.get()}), "test setup: writing a 1-flame fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(path));

    auto* list = window->findChild<QListWidget*>();
    auto* viewThumbnailsAction = window->findChild<QAction*>("viewThumbnailsAction");
    if (!check(list && list->count() == 1 && viewThumbnailsAction, "fixture listed and viewThumbnailsAction found")) {
        delete window;
        return;
    }
    check(list->item(0)->icon().isNull(), "starts with no thumbnail, having been listed while in list mode");

    viewThumbnailsAction->trigger();
    const bool gotIcon = waitUntil([list] { return !list->item(0)->icon().isNull(); });
    check(gotIcon, "switching to Thumbnails backfills a thumbnail for an entry that never had one requested");

    delete window;
    std::remove(path.c_str());
}

void testNewFlameAppliesConfiguredRenderDefaults() {
    RandomSettingsGuard guard;
    apo::ui::AppSettings::setDefaultGamma(3.3);
    apo::ui::AppSettings::setDefaultBrightness(5.5);
    apo::ui::AppSettings::setDefaultOversample(2);

    auto* window = new apo::ui::MainWindow();
    auto* newFlameAction = window->findChild<QAction*>("newFlameAction");
    if (!check(newFlameAction != nullptr, "MainWindow exposes a newFlameAction")) {
        delete window;
        return;
    }
    newFlameAction->trigger();
    auto* editor = window->findChild<apo::ui::EditorWindow*>();
    auto* saveFlameAsAction = editor ? editor->findChild<QAction*>("saveFlameAsAction") : nullptr;
    if (!check(editor && saveFlameAsAction, "New Flame opened an EditorWindow with a saveFlameAsAction")) {
        delete window;
        return;
    }

    // EditorWindow's constructor already kicked off its own initial preview
    // render asynchronously - see editSelectedFlameAndSaveTo's own comment
    // on why racing the modal Save dialog against it reliably hangs under
    // Linux/Docker's own thread scheduling.
    waitUntil([editor] { return !editor->statusBar()->currentMessage().contains("Rendering"); });

    const std::string path = "main_window_new_flame_defaults_test.flame";
    std::remove(path.c_str());
    acceptNextSaveDialogWith(editor, QString::fromStdString(path));
    saveFlameAsAction->trigger();

    auto loaded = apo::loadFlameFile(path);
    if (check(!loaded.empty(), "New Flame's Save Flame As writes a loadable file")) {
        check(approxEqual(loaded.front()->gamma, 3.3) && approxEqual(loaded.front()->brightness, 5.5) &&
                  loaded.front()->spatialOversample == 2,
              "a brand-new blank flame carries the configured render defaults (gamma/brightness/oversample), "
              "not Flame's own class defaults");
    }

    delete window;
    std::remove(path.c_str());
}

void testNewRandomBatchRespectsConfiguredRangeAndDefaults() {
    RandomSettingsGuard guard;
    apo::ui::AppSettings::setRandomMinXforms(3);
    apo::ui::AppSettings::setRandomMaxXforms(3); // deterministic count
    apo::ui::AppSettings::setRandomSymmetryType(0); // None - isolate this test from forced symmetry
    apo::ui::AppSettings::setRandomBatchTitlePrefix("Zzz");
    apo::ui::AppSettings::setDefaultGamma(7.0);

    auto* window = new apo::ui::MainWindow();
    auto* batchAction = window->findChild<QAction*>("newRandomBatchAction");
    auto* list = window->findChild<QListWidget*>();
    if (!check(batchAction && list, "newRandomBatchAction and list found")) {
        delete window;
        return;
    }

    whenModalShown<QInputDialog>(window, [](QInputDialog* dialog) {
        dialog->setIntValue(1);
        dialog->accept();
    });
    batchAction->trigger();
    if (!check(list->count() == 1, "the batch produced exactly 1 flame")) {
        delete window;
        return;
    }
    check(list->item(0)->text().startsWith("Zzz 1"), "the batch-generated flame's name uses the configured title prefix");
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });

    auto loaded = editSelectedFlameAndSaveTo(window, "main_window_random_batch_settings_test.flame");
    if (check(!loaded.empty(), "the batch flame's Save Flame As writes a loadable file")) {
        check(loaded.front()->numXForms() == 3,
              "the batch-generated flame's xform count matches the configured min=max=3 range");
        check(approxEqual(loaded.front()->gamma, 7.0), "the batch-generated flame carries the configured render defaults");
    }

    delete window;
    std::remove("main_window_random_batch_settings_test.flame");
}

void testNewRandomBatchAppliesConfiguredForcedSymmetry() {
    RandomSettingsGuard guard;
    apo::ui::AppSettings::setRandomMinXforms(1);
    apo::ui::AppSettings::setRandomMaxXforms(1); // exactly 1 xform before symmetry is applied
    apo::ui::AppSettings::setRandomSymmetryType(2); // Rotational
    apo::ui::AppSettings::setRandomSymmetryOrder(5);

    auto* window = new apo::ui::MainWindow();
    auto* batchAction = window->findChild<QAction*>("newRandomBatchAction");
    auto* list = window->findChild<QListWidget*>();
    if (!check(batchAction && list, "newRandomBatchAction and list found")) {
        delete window;
        return;
    }

    whenModalShown<QInputDialog>(window, [](QInputDialog* dialog) {
        dialog->setIntValue(1);
        dialog->accept();
    });
    batchAction->trigger();
    if (!check(list->count() == 1, "the batch produced exactly 1 flame")) {
        delete window;
        return;
    }
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });

    auto loaded = editSelectedFlameAndSaveTo(window, "main_window_random_batch_symmetry_test.flame");
    if (check(!loaded.empty(), "the symmetric batch flame's Save Flame As writes a loadable file")) {
        check(loaded.front()->numXForms() == 5,
              "rotational order-5 forced symmetry on a 1-xform random flame yields 1 + (5-1) = 5 total xforms "
              "(matches core/edit/Symmetry.h's addSymmetry(), already unit-tested independently in symmetry_test.cpp)");
    }

    delete window;
    std::remove("main_window_random_batch_symmetry_test.flame");
}

void testNewRandomBatchAppliesConfiguredForcedVariationAndGradientSource() {
    RandomSettingsGuard guard;
    apo::ui::AppSettings::setRandomMinXforms(2);
    apo::ui::AppSettings::setRandomMaxXforms(2);
    apo::ui::AppSettings::setRandomSymmetryType(0); // None - isolate this test from forced symmetry
    const auto& registry = apo::VariationRegistry::instance();
    const QString forcedName = QString::fromStdString(registry.varName(5)); // horseshoe
    apo::ui::AppSettings::setRandomForcedVariationName(forcedName);
    apo::ui::AppSettings::setRandomGradientSource(0); // Random preset

    auto* window = new apo::ui::MainWindow();
    auto* batchAction = window->findChild<QAction*>("newRandomBatchAction");
    auto* list = window->findChild<QListWidget*>();
    if (!check(batchAction && list, "newRandomBatchAction and list found")) {
        delete window;
        return;
    }

    whenModalShown<QInputDialog>(window, [](QInputDialog* dialog) {
        dialog->setIntValue(1);
        dialog->accept();
    });
    batchAction->trigger();
    if (!check(list->count() == 1, "the batch produced exactly 1 flame")) {
        delete window;
        return;
    }
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });

    auto loaded = editSelectedFlameAndSaveTo(window, "main_window_random_batch_variation_gradient_test.flame");
    if (check(!loaded.empty(), "the batch flame's Save Flame As writes a loadable file")) {
        const auto& flame = *loaded.front();
        const int forcedIndex = registry.variationIndex(forcedName.toStdString());
        bool everyXformUsesForced = true;
        for (int i = 0; i < flame.numXForms(); ++i) {
            if (!approxEqual(flame.xform[i]->variation(forcedIndex), 1.0)) everyXformUsesForced = false;
        }
        check(everyXformUsesForced,
              "the batch-generated flame's xforms all use the configured forced variation (Options > Random)");

        bool matchesABuiltin = false;
        for (int i = 0; i < apo::builtinGradientCount() && !matchesABuiltin; ++i) {
            if (flame.cmap.entries == apo::builtinGradient(i).entries) matchesABuiltin = true;
        }
        check(matchesABuiltin, "the batch-generated flame's gradient is an exact copy of a built-in preset, "
                                "matching the configured Random preset gradient source");
    }

    delete window;
    std::remove("main_window_random_batch_variation_gradient_test.flame");
}

void testNewRandomBatchRespectsConfiguredDisabledVariations() {
    RandomSettingsGuard guard;
    apo::ui::AppSettings::setRandomMinXforms(3);
    apo::ui::AppSettings::setRandomMaxXforms(3);
    apo::ui::AppSettings::setRandomSymmetryType(0); // None
    apo::ui::AppSettings::setRandomForcedVariationName(QString()); // Random - so the eligible-list actually matters
    // Pin weight to exactly 1.0 - this test's own focus is the eligible-
    // variations restriction, not the (separately tested) weight-range
    // feature, and a non-degenerate range would make the exact-1.0
    // assertion below fail even though the eligible-list restriction it's
    // actually checking still worked correctly.
    apo::ui::AppSettings::setRandomVariationWeightMin(1.0);
    apo::ui::AppSettings::setRandomVariationWeightMax(1.0);
    const auto& registry = apo::VariationRegistry::instance();
    const QString onlyEnabledName = QString::fromStdString(registry.varName(6)); // polar
    QStringList disabled;
    for (int i = 0; i < registry.nrVar(); ++i) {
        const QString name = QString::fromStdString(registry.varName(i));
        if (name != onlyEnabledName) disabled << name;
    }
    apo::ui::AppSettings::setDisabledVariationNames(disabled);

    auto* window = new apo::ui::MainWindow();
    auto* batchAction = window->findChild<QAction*>("newRandomBatchAction");
    auto* list = window->findChild<QListWidget*>();
    if (!check(batchAction && list, "newRandomBatchAction and list found")) {
        delete window;
        return;
    }

    whenModalShown<QInputDialog>(window, [](QInputDialog* dialog) {
        dialog->setIntValue(1);
        dialog->accept();
    });
    batchAction->trigger();
    if (!check(list->count() == 1, "the batch produced exactly 1 flame")) {
        delete window;
        return;
    }
    waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });

    auto loaded = editSelectedFlameAndSaveTo(window, "main_window_random_batch_disabled_variations_test.flame");
    if (check(!loaded.empty(), "the batch flame's Save Flame As writes a loadable file")) {
        const int onlyEnabledIndex = registry.variationIndex(onlyEnabledName.toStdString());
        check(approxEqual(loaded.front()->xform[0]->variation(onlyEnabledIndex), 1.0),
              "with every other variation disabled in Options, the random draw lands on the one still-enabled "
              "variation (Options > Variations checklist)");
    }

    delete window;
    std::remove("main_window_random_batch_disabled_variations_test.flame");
}

void testCameraGestureOnPreviewLabelMutatesTheSelectedFlameAndReRenders() {
    RandomSettingsGuard guard;
    apo::ui::AppSettings::setRandomMinXforms(1);
    apo::ui::AppSettings::setRandomMaxXforms(1);
    apo::ui::AppSettings::setRandomSymmetryType(0);

    auto* window = new apo::ui::MainWindow();
    auto* batchAction = window->findChild<QAction*>("newRandomBatchAction");
    auto* list = window->findChild<QListWidget*>();
    auto* previewLabel = window->findChild<apo::ui::PreviewLabel*>();
    if (!check(batchAction && list && previewLabel, "newRandomBatchAction, list, and PreviewLabel found")) {
        delete window;
        return;
    }

    whenModalShown<QInputDialog>(window, [](QInputDialog* dialog) {
        dialog->setIntValue(1);
        dialog->accept();
    });
    batchAction->trigger();
    if (!check(list->count() == 1, "the batch produced exactly 1 flame")) {
        delete window;
        return;
    }

    // The batch's initial selection already triggered a (full-quality)
    // render - wait for it to actually complete before testing the wheel
    // gesture: PreviewLabel ignores every gesture until it knows the
    // rendered image's real pixel size (setRenderedImageSize(), only
    // called from MainWindow::onRenderFinished once a render's result
    // actually arrives back from RenderWorker's own thread).
    const bool initialRenderSettled =
        waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    if (!check(initialRenderSettled, "the batch flame's initial preview render completes")) {
        delete window;
        return;
    }

    QSignalSpy renderSpy(window, &apo::ui::MainWindow::renderRequested);
    QWheelEvent wheel(QPointF(50, 50), previewLabel->mapToGlobal(QPoint(50, 50)), QPoint(0, 0), QPoint(0, 120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(previewLabel, &wheel);
    check(renderSpy.count() >= 1, "a wheel-zoom gesture on the preview requests a fresh render");

    const bool gestureRenderSettled =
        waitUntil([window] { return !window->statusBar()->currentMessage().contains("Rendering"); });
    check(gestureRenderSettled, "the wheel gesture's re-render completes");

    auto loaded = editSelectedFlameAndSaveTo(window, "main_window_camera_gesture_test.flame");
    if (check(!loaded.empty(), "the flame's Save Flame As writes a loadable file after the camera gesture")) {
        check(!approxEqual(loaded.front()->zoom, 0.0),
              "the wheel gesture's Flame::zoom change reached the actual selected flame, not just a preview clone");
    }

    delete window;
    std::remove("main_window_camera_gesture_test.flame");
}

void testMainPreviewShowsProgressDuringAFullQualityRender() {
    // Large/dense enough that the render takes a real, perceptible amount
    // of time - the tiny fixtures elsewhere in this file finish too fast
    // for onProgressTick()'s ~150ms poll to ever land a single tick before
    // the render's already done. ~9M points (density=400) was that margin
    // when this test was written, but Part B's cumulative render-path
    // speedups (B1a/B2/B3/B4 - render-clone slimming, hot-loop dispatch,
    // skipping the opacity roll when transOpacity==1, parallel reduce/
    // tonemap) made that stale: caught directly, not assumed - this test
    // started failing consistently (not flakily) once those items landed,
    // because the render now finishes before the first progress tick fires.
    // 4x the original point count restores real margin against the render
    // path being fast on any given run, current or future.
    //
    // openFlameFile() now applies AppSettings::defaultSampleDensity() to
    // every loaded flame (same reset New Flame/New Random Batch already
    // got - see MainWindow::openFlameFile's own comment on why loading a
    // flame saved at a high final-export density shouldn't make every
    // preview render that slow too), so the fixture's own sampleDensity
    // below no longer survives the load - raising the *setting* is what
    // actually controls it now.
    const double savedDefaultSampleDensity = apo::ui::AppSettings::defaultSampleDensity();
    apo::ui::AppSettings::setDefaultSampleDensity(1600); // ~36M points - see comment above on why 400 stopped being enough

    auto flame = makeNamedFlame("Alpha");
    flame->width = 150;
    flame->height = 150;

    const std::string path = "main_window_progress_test.flame";
    std::remove(path.c_str());
    check(apo::saveFlameFile(path, {flame.get()}), "test setup: writing a fixture succeeds");

    auto* window = new apo::ui::MainWindow();
    window->openFlameFile(QString::fromStdString(path)); // triggers the initial full-quality preview render

    const bool sawProgress = waitUntil(
        [window] {
            const QString msg = window->statusBar()->currentMessage();
            return msg.contains("Rendering...") && msg.contains("%");
        },
        5000);
    check(sawProgress,
          "the main preview's full-quality render shows a live percent/elapsed/remaining status (plan's P2.5), "
          "matching Main.pas's own OnProgress panels");

    const bool settled = waitUntil([window] { return window->statusBar()->currentMessage().contains("Rendered"); }, 15000);
    check(settled, "the status bar still reports the final \"Rendered...\" message once the render completes");

    delete window;
    std::remove(path.c_str());
    apo::ui::AppSettings::setDefaultSampleDensity(savedDefaultSampleDensity);
}

void testAboutDialogOpensAndClosesModally() {
    auto* window = new apo::ui::MainWindow();
    auto* aboutAction = window->findChild<QAction*>("aboutAction");
    if (!check(aboutAction != nullptr, "MainWindow exposes an aboutAction")) {
        delete window;
        return;
    }

    bool foundAboutDialog = false;
    whenModalShown<QDialog>(window, [&foundAboutDialog](QDialog* dialog) {
        foundAboutDialog = dialog->windowTitle().contains("About");
        dialog->accept();
    });
    aboutAction->trigger(); // openAboutDialog() calls exec() - blocks until the timer above closes it

    check(foundAboutDialog, "triggering About opens a modal dialog titled 'About ...' and it closes cleanly");

    delete window;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    // Required for AppSettings (QSettings-backed) to resolve a real storage
    // location - without these, QSettings silently no-ops every read/write
    // (status() == AccessError) instead of throwing or asserting, so this
    // was easy to miss until a test actually depended on a persisted value
    // round-tripping. Matches options_dialog_interaction_test.cpp's setup.
    QApplication::setApplicationName("Apophysis 7X");
    QApplication::setOrganizationName("Apophysis 7X");

    testNewFlameOpensEditableBlankFlame();
    testSaveFlameAsFromMainWindowRoundTrips();
    testSaveAllFlamesRoundTripsEveryFlameNotJustSelected();
    testSaveAllFlamesDisabledActionStillPresentWhenLibraryEmpty();
    testCopyPutsSelectedFlameXmlOnTheClipboard();
    testPasteInsertsAFreshLibraryEntryFromClipboardXml();
    testPasteIntoEmptyLibraryInsertsAtRowZero();
    testPasteWithGarbageClipboardShowsAWarningAndChangesNothing();
    testRenderAllFlamesShowsInfoBoxWhenLibraryEmpty();
    testRenderAllFlamesOpensDialogWithTheWholeLibrary();
    testUndoRedoActionsStartDisabledAndHaveStandardShortcuts();
    testUndoRedoResetLocation();
    testUndoRedoRename();
    testUndoRedoDelete();
    testUndoRedoPaste();
    testUndoRedoCameraGestureSettle();
    testUndoStackIsBoundedTo32Entries();
    testSmoothPaletteActionGatedOnSelection();
    testSmoothPaletteAppliesToSelectedFlameWithUndo();
    testDroppingAFlameFileOpensItLikeFileOpen();
    testDragEnterAcceptsLocalFileUrlsAndRejectsPlainText();
    testDroppingAMultiFlameFileListsAllOfThem();
    testDroppingAnUnparseableFileShowsAWarningAndChangesNothing();
    testSaveFlameAsFromEditorWindowRoundTripsEdits();
    testNewRandomBatchGeneratesFramedFlames();
    testNewRandomBatchHasCtrlBShortcut();
    testEditFlameActionOpensEditorForARandomBatchFlame();
    testLibraryActionsStartDisabledUntilSelection();
    testRenameCommitsToTheUnderlyingFlameOnly();
    testDeleteRemovesEntryFromLibraryWithConfirmation();
    testDeleteCancelledLeavesLibraryUnchanged();
    testDuplicateFlameInsertsCopyAfterOriginalAndSelectsIt();
    testDuplicateFlameCopiesEveryFieldViaClone();
    testResetLocationRecomputesZoomAndCenter();
    testResetLocationHasF12Shortcut();
    testViewModeDefaultsToThumbnailsAndTogglesTheListWidgetViewMode();
    testListViewSkipsThumbnailGenerationForNewEntries();
    testSwitchingToThumbnailViewBackfillsExistingEntries();
    testNewFlameAppliesConfiguredRenderDefaults();
    testNewRandomBatchRespectsConfiguredRangeAndDefaults();
    testNewRandomBatchAppliesConfiguredForcedSymmetry();
    testNewRandomBatchAppliesConfiguredForcedVariationAndGradientSource();
    testNewRandomBatchRespectsConfiguredDisabledVariations();
    testCameraGestureOnPreviewLabelMutatesTheSelectedFlameAndReRenders();
    testMainPreviewShowsProgressDuringAFullQualityRender();
    testAboutDialogOpensAndClosesModally();

    return apo_test::reportAndExit();
}
