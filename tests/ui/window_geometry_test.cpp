// Real-widget interaction test for window-size persistence
// (src/ui/WindowGeometry.h + AppSettings::windowGeometry): every
// independently resizable top-level window/dialog restores its last-used
// size (and position) via restoreWindowGeometry() in its constructor, and
// saves it via saveWindowGeometry() in a closeEvent() override - so
// resizing a window, closing it, and reopening it (or relaunching the app)
// brings it back at the size it was last resized to. Covers all four
// combinations this mechanism is wired into: QMainWindow without
// WA_DeleteOnClose (MainWindow), QMainWindow with WA_DeleteOnClose
// (EditorWindow), QDialog with WA_DeleteOnClose and a Flame-taking
// constructor (PostProcessDialog), and QDialog without WA_DeleteOnClose and
// a no-arg constructor (OptionsDialog) - the other 8 wired-up dialogs
// (AdjustDialog, CurvesDialog, RenderDialog, RenderAllDialog, MutateDialog,
// GradientBrowserDialog, XaosDialog, SmoothPaletteDialog) follow the exact
// same three-line pattern in each of these four already covered, so aren't
// independently re-tested here. PostProcessDialog (not AdjustDialog) was
// picked for the Flame-taking QDialog case specifically because
// AdjustDialog's own four-tab control panel has a real layout-enforced
// minimum width (~876px, measured directly) that exceeds the offscreen QPA
// platform's fixed 800px-wide virtual screen - QWidget::resize() doesn't
// clamp to the screen, but QWidget::restoreGeometry() does (deliberately,
// to keep a restored window from opening off-screen), so the two disagree
// once a window's natural size exceeds the available screen and no target
// size can round-trip cleanly. PostProcessDialog's simpler single-panel
// control layout stays comfortably under that ceiling.

#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>

#include <QApplication>
#include <QByteArray>
#include <QScreen>
#include <QSize>
#include <QTest>
#include <QWidget>

#include "../TestHelpers.h"
#include "AppSettings.h"
#include "EditorWindow.h"
#include "MainWindow.h"
#include "OptionsDialog.h"
#include "PostProcessDialog.h"
#include "core/Flame.h"

using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->name = "window geometry test flame";
    flame->width = 150;
    flame->height = 150;
    return flame;
}

// `deleteOnClose` matters for cleanup only: WA_DeleteOnClose windows
// self-destruct (deferred) once close() is called, so they must not also
// be delete'd directly; the rest self-manage via a plain `delete`.
void checkGeometryRoundTrip(const char* windowKey, const QSize& targetSize,
                             const std::function<QWidget*()>& makeWindow, bool deleteOnClose) {
    const QByteArray savedGeometry = apo::ui::AppSettings::windowGeometry(windowKey);

    QWidget* first = makeWindow();
    first->show();
    first->resize(targetSize);
    QTest::qWait(20);
    const QSize resizedTo = first->size();
    first->close();
    QTest::qWait(20); // lets a WA_DeleteOnClose window's deferred deleteLater() run
    if (!deleteOnClose) delete first;

    const QByteArray afterClose = apo::ui::AppSettings::windowGeometry(windowKey);
    check(afterClose != savedGeometry,
          (std::string(windowKey) + ": closing after a resize saves a new geometry").c_str());

    QWidget* second = makeWindow();
    if (second->size() != resizedTo) {
        // Diagnostic-only: this mismatch has so far only been reproduced on
        // Linux/macOS CI (the offscreen QPA platform's virtual screen size
        // isn't guaranteed identical across Qt versions/platforms - see
        // this file's own top comment on the AdjustDialog precedent), never
        // locally on Windows, so print everything relevant on failure
        // rather than guess again from a bare pass/fail.
        const QScreen* screen = QGuiApplication::primaryScreen();
        const QRect screenGeom = screen ? screen->availableGeometry() : QRect();
        std::fprintf(stderr,
                     "DEBUG %s size mismatch: target=%dx%d resizedTo(first)=%dx%d second->size()=%dx%d "
                     "second->minimumSizeHint()=%dx%d screen.availableGeometry=%dx%d+%d+%d\n",
                     windowKey, targetSize.width(), targetSize.height(), resizedTo.width(), resizedTo.height(),
                     second->size().width(), second->size().height(), second->minimumSizeHint().width(),
                     second->minimumSizeHint().height(), screenGeom.width(), screenGeom.height(), screenGeom.x(),
                     screenGeom.y());
    }
    check(second->size() == resizedTo, (std::string(windowKey) + ": reopening restores the last-used size").c_str());
    second->close();
    QTest::qWait(20);
    if (!deleteOnClose) delete second;

    // Restore whatever was actually persisted before this test ran (empty
    // QByteArray if nothing was), so running this test doesn't permanently
    // change the real app's remembered window sizes.
    apo::ui::AppSettings::setWindowGeometry(windowKey, savedGeometry);
}

void testMainWindowRemembersSize(const QSize& targetSize) {
    checkGeometryRoundTrip(
        "MainWindow", targetSize, [] { return new apo::ui::MainWindow(); }, /*deleteOnClose=*/false);
}

void testEditorWindowRemembersSize(const QSize& targetSize) {
    checkGeometryRoundTrip(
        "EditorWindow", targetSize, [] { return new apo::ui::EditorWindow(makeTestFlame()); },
        /*deleteOnClose=*/true);
}

void testPostProcessDialogRemembersSize(const QSize& targetSize) {
    checkGeometryRoundTrip(
        "PostProcessDialog", targetSize, [] { return new apo::ui::PostProcessDialog(makeTestFlame()); },
        /*deleteOnClose=*/true);
}

void testOptionsDialogRemembersSize(const QSize& targetSize) {
    checkGeometryRoundTrip(
        "OptionsDialog", targetSize, [] { return new apo::ui::OptionsDialog(); }, /*deleteOnClose=*/false);
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    // Required for AppSettings (QSettings-backed) to resolve a real storage
    // location - see AppSettings.h and main_window_interaction_test.cpp's
    // own main() for the same requirement.
    QApplication::setApplicationName("Apophysis 7X");
    QApplication::setOrganizationName("Apophysis 7X");

    // QWidget::restoreGeometry() (unlike a plain resize()) clamps a saved
    // size back down to fit the screen it's restoring onto - the offscreen
    // QPA platform's default virtual screen is a modest, fixed size (800x800
    // as of Qt 6.8), smaller than several of these windows' own hardcoded
    // default sizes (MainWindow's 1100x750, EditorWindow's 1000x700). A
    // fixed literal target size here would silently get clamped on restore,
    // making the "did it actually restore" check below pass or fail based
    // on the test environment's screen size rather than the mechanism under
    // test - so it's derived from the real available screen instead, with
    // enough margin to stay clear of the clamp on any platform.
    const QScreen* screen = QGuiApplication::primaryScreen();
    const QSize screenSize = screen ? screen->availableGeometry().size() : QSize(800, 800);
    const QSize targetSize(std::min(700, screenSize.width() - 80), std::min(560, screenSize.height() - 80));

    testMainWindowRemembersSize(targetSize);
    testEditorWindowRemembersSize(targetSize);
    testPostProcessDialogRemembersSize(targetSize);
    testOptionsDialogRemembersSize(targetSize);

    return apo_test::reportAndExit();
}
