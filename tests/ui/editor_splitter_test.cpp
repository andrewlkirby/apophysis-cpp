// Real-widget interaction test for the flame editor's Transform/Adjust
// sidebar width persistence (EditorWindow's centralSplitter_ +
// AppSettings::splitterState): dragging the splitter handle to a new width,
// closing the editor, and reopening it (or relaunching the app) should
// bring the sidebar back at the width it was last resized to, the same way
// window_geometry_test.cpp already covers each window's own outer size.

#include <algorithm>
#include <memory>

#include <QApplication>
#include <QByteArray>
#include <QScreen>
#include <QSize>
#include <QSplitter>
#include <QTest>

#include "../TestHelpers.h"
#include "AppSettings.h"
#include "EditorWindow.h"
#include "core/Flame.h"

using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->name = "editor splitter test flame";
    flame->width = 150;
    flame->height = 150;
    return flame;
}

void testEditorWindowRemembersSidebarWidth(const QSize& safeWindowSize) {
    const QByteArray savedState = apo::ui::AppSettings::splitterState("EditorWindow");

    auto* first = new apo::ui::EditorWindow(makeTestFlame());
    first->show();
    // EditorWindow's own constructor defaults to resize(1000, 700) - wider
    // than the offscreen QPA platform's ~800px-wide virtual screen (see
    // window_geometry_test.cpp's own top comment on this exact clamp).
    // resize() itself doesn't clamp to the screen, only restoreGeometry()
    // does, so leaving this window at its unclamped 1000-wide default while
    // the *second* window below restores a saved geometry that DOES get
    // clamped would compare two windows at genuinely different real widths -
    // nothing to do with the splitter-restore mechanism this test is
    // actually meant to cover. Explicitly resizing down to a screen-safe
    // size here keeps both windows at the same real width.
    first->resize(safeWindowSize);
    QTest::qWait(20);

    auto* firstSplitter = first->findChild<QSplitter*>("centralSplitter");
    check(firstSplitter != nullptr, "EditorWindow exposes its central splitter as \"centralSplitter\"");

    // Comfortably outside the {720, 280} default the constructor seeds on
    // every fresh open (EditorWindow.cpp), within rightContainer's
    // [280, 900] min/max width clamp, and leaving canvas_ at least its own
    // 200px hard minimum (TriangleCanvas::setMinimumSize) - derived from
    // `total` rather than a flat pixel value since safeWindowSize above
    // (and so `total`) can be a good deal narrower than a real display
    // under the offscreen QPA platform's small virtual screen.
    const int total = firstSplitter->width();
    const int targetSidebarWidth = std::clamp(total - 250, 280, 900);
    firstSplitter->setSizes({total - targetSidebarWidth, targetSidebarWidth});
    QTest::qWait(20);
    const int resizedSidebarWidth = firstSplitter->sizes().value(1);

    first->close();
    QTest::qWait(20); // lets EditorWindow's WA_DeleteOnClose deferred deleteLater() run

    const QByteArray afterClose = apo::ui::AppSettings::splitterState("EditorWindow");
    check(afterClose != savedState, "closing the editor after a splitter drag saves a new splitter state");

    auto* second = new apo::ui::EditorWindow(makeTestFlame());
    second->show();
    QTest::qWait(20);

    auto* secondSplitter = second->findChild<QSplitter*>("centralSplitter");
    check(secondSplitter != nullptr, "the reopened EditorWindow also exposes \"centralSplitter\"");
    check(secondSplitter->sizes().value(1) == resizedSidebarWidth,
          "reopening the editor restores the sidebar to its last-dragged width");

    second->close();
    QTest::qWait(20);

    // Restore whatever was actually persisted before this test ran (empty
    // QByteArray if nothing was), so running this test doesn't permanently
    // change the real app's remembered sidebar width.
    apo::ui::AppSettings::setSplitterState("EditorWindow", savedState);
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setApplicationName("Apophysis 7X");
    QApplication::setOrganizationName("Apophysis 7X");

    // Same margin-clamped derivation window_geometry_test.cpp uses for the
    // same reason (see its own top comment) - keeps this test's window at a
    // size that round-trips through QWidget::restoreGeometry() unclamped on
    // the offscreen QPA platform's small virtual screen.
    const QScreen* screen = QGuiApplication::primaryScreen();
    const QSize screenSize = screen ? screen->availableGeometry().size() : QSize(800, 800);
    const QSize safeWindowSize(std::min(1000, screenSize.width() - 80), std::min(700, screenSize.height() - 80));

    testEditorWindowRemembersSidebarWidth(safeWindowSize);

    return apo_test::reportAndExit();
}
