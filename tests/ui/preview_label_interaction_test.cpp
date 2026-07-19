// Real-widget interaction tests for PreviewLabel (src/ui/PreviewLabel.h) -
// MainWindow's library preview, upgraded from pure display to a directly-
// interactive camera (plan's P2.1: mouse-wheel zoom + drag pan writing
// straight into the selected flame's own Flame::zoom/center). Mirrors
// triangle_canvas_interaction_test.cpp's approach: verify the *invariants*
// each gesture is supposed to hold (a cursor-anchored zoom keeps the same
// flame-space point under the cursor; a drag makes the content that was
// under the drag's start position end up under its end position) rather
// than hardcoding expected zoom/center numbers - that would just be a
// second copy of PreviewLabel.cpp's own formula, at risk of copying a bug
// right along with it.

#include <cmath>

#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>

#include "../TestHelpers.h"
#include "PreviewLabel.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::shared_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->width = 200;
    flame->height = 200;
    flame->pixelsPerUnit = 50;
    flame->zoom = 0.0;
    flame->center = {0.0, 0.0};
    return flame;
}

// Independently-statable projection formula (matches Renderer.cpp's
// buildBuffersAndCamera, and is documented identically in
// PreviewLabel::widgetToFlame's own comment) - NOT a call into
// PreviewLabel's own (private) widgetToFlame(), so a test built on this
// actually exercises whether the widget's real behavior matches the
// documented contract, rather than just echoing back whatever the
// implementation already computed.
QPointF imagePixelToFlame(const apo::Flame& flame, double imageC, double imageR) {
    const double scale = std::pow(2.0, flame.zoom);
    const double ppux = flame.pixelsPerUnit * scale;
    const double ppuy = flame.pixelsPerUnit * scale;
    return QPointF(flame.center[0] + (imageC - flame.width / 2.0) / ppux,
                    flame.center[1] - (imageR - flame.height / 2.0) / ppuy);
}

// A 1:1-scaled, unlettered label: displayed pixmap size == renderedImageSize
// == label size, so widget-local coordinates equal image-pixel coordinates
// directly, keeping every test's arithmetic simple.
void setUp1To1(apo::ui::PreviewLabel& label, std::shared_ptr<apo::Flame> flame) {
    label.resize(200, 200);
    QPixmap pm(200, 200);
    pm.fill(Qt::black);
    label.setPixmap(pm);
    label.setRenderedImageSize(QSize(200, 200));
    label.setFlame(std::move(flame));
}

void testWheelZoomIsAnchoredOnTheCursor() {
    auto flame = makeTestFlame();
    apo::ui::PreviewLabel label;
    setUp1To1(label, flame);

    // Image pixel (150, 100) - off-center, so an anchored zoom actually
    // moves Flame::center (a center-only zoom would leave it at (0,0)).
    const QPointF cursorWidgetPos(150, 100);
    const QPointF cursorFlameBefore = imagePixelToFlame(*flame, cursorWidgetPos.x(), cursorWidgetPos.y());

    QWheelEvent wheel(cursorWidgetPos, label.mapToGlobal(cursorWidgetPos.toPoint()), QPoint(0, 0), QPoint(0, 120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&label, &wheel);

    check(flame->zoom > 0.0, "scrolling up (positive angleDelta) increases Flame::zoom - zooms in");
    check(!(approxEqual(flame->center[0], 0.0) && approxEqual(flame->center[1], 0.0)),
          "an off-center cursor position actually moves Flame::center, not just Flame::zoom");

    const QPointF cursorFlameAfter = imagePixelToFlame(*flame, cursorWidgetPos.x(), cursorWidgetPos.y());
    check(approxEqual(cursorFlameBefore.x(), cursorFlameAfter.x(), 1e-6) &&
              approxEqual(cursorFlameBefore.y(), cursorFlameAfter.y(), 1e-6),
          "the flame-space point under the cursor is unchanged by the zoom - the defining property of a "
          "cursor-anchored zoom, not just a center-anchored one");
}

void testWheelScrollDownZoomsOut() {
    auto flame = makeTestFlame();
    apo::ui::PreviewLabel label;
    setUp1To1(label, flame);

    QWheelEvent wheel(QPointF(100, 100), QPointF(100, 100), QPoint(0, 0), QPoint(0, -120), Qt::NoButton,
                       Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&label, &wheel);

    check(flame->zoom < 0.0, "scrolling down (negative angleDelta) decreases Flame::zoom - zooms out");
}

void testWheelEmitsBothCameraSignalsExactlyOnce() {
    auto flame = makeTestFlame();
    apo::ui::PreviewLabel label;
    setUp1To1(label, flame);

    QSignalSpy changedSpy(&label, &apo::ui::PreviewLabel::cameraChanged);
    QSignalSpy finishedSpy(&label, &apo::ui::PreviewLabel::cameraChangeFinished);

    QWheelEvent wheel(QPointF(100, 100), QPointF(100, 100), QPoint(0, 0), QPoint(0, 120), Qt::NoButton,
                       Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&label, &wheel);

    check(changedSpy.count() == 1, "a single wheel notch fires cameraChanged exactly once");
    check(finishedSpy.count() == 1,
          "a single wheel notch also fires cameraChangeFinished - already a complete, discrete gesture, unlike a drag");
}

void testDragPansContentToFollowTheCursor() {
    auto flame = makeTestFlame();
    apo::ui::PreviewLabel label;
    setUp1To1(label, flame);
    label.show();

    const QPoint startPx(100, 100);
    const QPoint endPx(140, 70);
    const QPointF contentAtStart = imagePixelToFlame(*flame, startPx.x(), startPx.y());

    QTest::mousePress(&label, Qt::LeftButton, Qt::NoModifier, startPx);
    QTest::mouseMove(&label, endPx);
    QTest::mouseRelease(&label, Qt::LeftButton, Qt::NoModifier, endPx);

    // "Content follows the cursor": whatever flame-space point was under
    // the drag's start position should, after the drag, be the point now
    // rendered at the drag's *end* position - i.e. the same widget-pixel
    // motion the mouse made.
    const QPointF contentNowAtEnd = imagePixelToFlame(*flame, endPx.x(), endPx.y());
    check(approxEqual(contentAtStart.x(), contentNowAtEnd.x(), 1e-6) &&
              approxEqual(contentAtStart.y(), contentNowAtEnd.y(), 1e-6),
          "dragging from A to B leaves the flame-space point originally under A now rendered under B - "
          "'grab and drag' panning, matching how image/map viewers behave");

    check(!(approxEqual(flame->center[0], 0.0) && approxEqual(flame->center[1], 0.0)),
          "the drag actually moved Flame::center");
}

void testDragEmitsCameraChangedPerMoveAndFinishedOnRelease() {
    auto flame = makeTestFlame();
    apo::ui::PreviewLabel label;
    setUp1To1(label, flame);
    label.show();

    QSignalSpy changedSpy(&label, &apo::ui::PreviewLabel::cameraChanged);
    QSignalSpy finishedSpy(&label, &apo::ui::PreviewLabel::cameraChangeFinished);

    QTest::mousePress(&label, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    check(changedSpy.count() == 0 && finishedSpy.count() == 0, "pressing alone (no movement yet) fires neither signal");

    QTest::mouseMove(&label, QPoint(110, 100));
    QTest::mouseMove(&label, QPoint(120, 105));
    check(changedSpy.count() == 2, "cameraChanged fires once per mouse-move while dragging (two moves here)");
    check(finishedSpy.count() == 0, "cameraChangeFinished does not fire mid-drag");

    QTest::mouseRelease(&label, Qt::LeftButton, Qt::NoModifier, QPoint(120, 105));
    check(finishedSpy.count() == 1, "cameraChangeFinished fires exactly once, on release");
}

void testGesturesAreNoOpsWithoutAFlame() {
    apo::ui::PreviewLabel label;
    label.resize(200, 200);
    label.setRenderedImageSize(QSize(200, 200));
    label.show();
    // Deliberately never calls setFlame() - matches PreviewLabel's state
    // right after construction, or after MainWindow deselects everything.

    QSignalSpy changedSpy(&label, &apo::ui::PreviewLabel::cameraChanged);
    QSignalSpy finishedSpy(&label, &apo::ui::PreviewLabel::cameraChangeFinished);

    QWheelEvent wheel(QPointF(100, 100), QPointF(100, 100), QPoint(0, 0), QPoint(0, 120), Qt::NoButton,
                       Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&label, &wheel);
    QTest::mousePress(&label, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    QTest::mouseMove(&label, QPoint(120, 110));
    QTest::mouseRelease(&label, Qt::LeftButton, Qt::NoModifier, QPoint(120, 110));

    check(changedSpy.count() == 0 && finishedSpy.count() == 0,
          "with no flame set, neither wheel nor drag gestures emit any camera signal (nothing to mutate)");
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testWheelZoomIsAnchoredOnTheCursor();
    testWheelScrollDownZoomsOut();
    testWheelEmitsBothCameraSignalsExactlyOnce();
    testDragPansContentToFollowTheCursor();
    testDragEmitsCameraChangedPerMoveAndFinishedOnRelease();
    testGesturesAreNoOpsWithoutAFlame();

    return apo_test::reportAndExit();
}
