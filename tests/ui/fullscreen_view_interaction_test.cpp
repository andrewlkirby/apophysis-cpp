// Real-widget interaction tests for FullscreenView (src/ui/
// FullscreenView.h) - the underlying render/cancel machinery
// (RenderWorker::renderFull, apo::RenderProgress) is already covered
// independently in renderer_test.cpp and render_dialog_interaction_test.cpp;
// this file verifies the Qt wiring on top of it: showing the view starts a
// render automatically and it completes with a real image, any key press or
// a double-click closes the view, and the right-click context menu's Cancel
// Render actually stops an in-flight render. Mirrors
// render_dialog_interaction_test.cpp's approach.

#include <functional>

#include <QApplication>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QLabel>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include "../TestHelpers.h"
#include "FullscreenView.h"

using apo_test::check;

namespace {

// Polls every 10ms (bounded by timeoutMs) until QApplication's active popup
// widget is a QMenu, then invokes `onShown` with it once and stops -
// replaces a single QTimer::singleShot(50, ...) check, which can race a
// popup that happens to open later than that hardcoded delay. This was a
// real, confirmed non-deterministic hang, not a theoretical concern - see
// FOLLOWUP_PLAN.txt's B7 entry and main_window_interaction_test.cpp's own
// copy of this pattern (there, against activeModalWidget() for real modal
// dialogs rather than a popup menu) for the full story. `context` is the
// QObject the polling timer is parented to, so it's cleaned up
// automatically if that object is destroyed first.
template <class F>
void whenPopupMenuShown(QObject* context, F onShown, int timeoutMs = 5000) {
    auto* timer = new QTimer(context);
    QElapsedTimer elapsed;
    elapsed.start();
    timer->setInterval(10);
    QObject::connect(timer, &QTimer::timeout, context, [timer, elapsed, onShown, timeoutMs]() mutable {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
            timer->stop();
            timer->deleteLater();
            onShown(menu);
        } else if (elapsed.elapsed() > timeoutMs) {
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start();
}

std::shared_ptr<apo::Flame> makeSierpinskiFlame(double density = 20) {
    auto flame = std::make_shared<apo::Flame>();
    flame->width = 48;
    flame->height = 48;
    flame->sampleDensity = density;
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

bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!pred()) {
        if (timer.elapsed() > timeoutMs) return false;
        QTest::qWait(20);
    }
    return true;
}

void testShowStartsRenderAndCompletes() {
    auto* view = new apo::ui::FullscreenView(makeSierpinskiFlame());
    auto* imageLabel = view->findChild<QLabel*>("fullscreenImageLabel");
    auto* statusLabel = view->findChild<QLabel*>("fullscreenStatusLabel");
    if (!check(imageLabel && statusLabel, "FullscreenView exposes its image and status labels")) {
        delete view;
        return;
    }

    view->showFullScreen();

    const bool finished = waitUntil([statusLabel] { return statusLabel->text().contains("Done"); });
    check(finished, "showing the view starts a render automatically and it reaches a Done state");
    check(!imageLabel->pixmap().isNull(), "the image label shows a real rendered pixmap once done");

    delete view;
}

void testAnyKeyPressCloses() {
    auto* view = new apo::ui::FullscreenView(makeSierpinskiFlame());
    auto* statusLabel = view->findChild<QLabel*>("fullscreenStatusLabel");
    view->showFullScreen();
    waitUntil([statusLabel] { return statusLabel->text().contains("Done"); });

    QSignalSpy spy(view, &QObject::destroyed);
    QTest::keyClick(view, Qt::Key_A);
    check(!view->isVisible(), "pressing any key closes the fullscreen view");
    // WA_DeleteOnClose schedules the actual delete for the next event-loop
    // turn, not synchronously within close() - pump briefly to observe it.
    waitUntil([&spy] { return spy.count() > 0; }, 2000);
    check(spy.count() > 0, "closing the view eventually deletes it (WA_DeleteOnClose)");
}

void testDoubleClickCloses() {
    auto* view = new apo::ui::FullscreenView(makeSierpinskiFlame());
    auto* statusLabel = view->findChild<QLabel*>("fullscreenStatusLabel");
    view->showFullScreen();
    waitUntil([statusLabel] { return statusLabel->text().contains("Done"); });

    QTest::mouseDClick(view, Qt::LeftButton);
    check(!view->isVisible(), "double-clicking closes the fullscreen view");

    delete view;
}

void testContextMenuCancelStopsRender() {
    // A somewhat higher density so the render takes a little time (giving
    // the test a real window to land the cancel in) without being
    // absurd - the offscreen QPA platform's screen is 800x800 (verified
    // directly), and density is a per-pixel multiplier, so anything much
    // higher than this makes even a *cancelled* run's one sub-batch of
    // warm-up work take unreasonably long.
    auto* view = new apo::ui::FullscreenView(makeSierpinskiFlame(200));
    auto* statusLabel = view->findChild<QLabel*>("fullscreenStatusLabel");
    if (!check(statusLabel != nullptr, "statusLabel found")) {
        delete view;
        return;
    }

    // Right-click opens the context menu, which runs its own nested modal
    // event loop (QMenu::exec) - poll for it and act once it's open
    // (whenPopupMenuShown), the standard pattern here for modal popups/
    // dialogs triggered by a click.
    whenPopupMenuShown(view, [](QMenu* menu) {
        for (QAction* action : menu->actions()) {
            if (action->text() == "Cancel Render") {
                action->trigger();
                break;
            }
        }
        menu->close();
    });

    view->showFullScreen();
    // QTest::mouseClick(view, Qt::RightButton) does NOT reliably trigger
    // contextMenuEvent() under the offscreen QPA platform (verified
    // directly - it silently never fires), so send the QContextMenuEvent
    // Qt itself would dispatch on a real right-click instead. showEvent()
    // (which starts the render and constructs progress_) has already run
    // synchronously by the time showFullScreen() returns, so this reaches
    // a render that's genuinely in flight.
    QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(10, 10), view->mapToGlobal(QPoint(10, 10)));
    QApplication::sendEvent(view, &event);

    const bool finished = waitUntil([statusLabel] { return !statusLabel->text().contains("Rendering"); });
    check(finished, "the render reaches a finished state after Cancel Render");
    check(statusLabel->text().contains("Cancelled"), "Cancel Render in the context menu actually stops the render");

    delete view;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testShowStartsRenderAndCompletes();
    testAnyKeyPressCloses();
    testDoubleClickCloses();
    testContextMenuCancelStopsRender();

    return apo_test::reportAndExit();
}
