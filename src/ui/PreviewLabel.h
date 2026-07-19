#pragma once

#include <memory>

#include <QLabel>

#include "core/Flame.h"

namespace apo::ui {

// MainWindow's library preview, upgraded from pure display (plan's P2.1)
// to a directly-interactive camera: mouse-wheel zoom (anchored on the
// cursor, not just the view center) and click-drag pan, both writing
// straight into Flame::zoom/center and asking the owner to re-render -
// the same "own the Flame, mutate it directly, emit a signal" shape
// TriangleCanvas already established for the Editor's triangle drags,
// scaled down to the two gestures actually worth porting here (plan's own
// wording: "skip rotate mode and the rectangle-zoom variants initially").
//
// Unlike TriangleCanvas, there is no separate "view camera" here - this
// widget edits the flame's own real render camera (Flame::zoom/center)
// directly, since the whole point of this preview is "what you see is
// what a full render will look like" - there's nothing else for a camera
// gesture to mean.
class PreviewLabel final : public QLabel {
    Q_OBJECT

public:
    explicit PreviewLabel(QWidget* parent = nullptr);

    // Called whenever MainWindow's selection changes - the same Flame
    // instance is then mutated in place by every subsequent gesture until
    // the next call (matches TriangleCanvas::setFlame's contract). nullptr
    // disables every gesture below (nothing selected to edit).
    void setFlame(std::shared_ptr<apo::Flame> flame);

    // The pixel dimensions the currently-displayed pixmap was actually
    // *rendered* at - needed to convert a widget-local mouse position into
    // the render's own coordinate space (see widgetToFlame()). Set by the
    // owner (MainWindow) each time a fresh render arrives: this widget
    // never renders anything itself, and the pixmap it *displays* is
    // scaled to fit the label, so it's a different size than the image
    // that was actually rendered - this is how the two get reconciled.
    void setRenderedImageSize(QSize size);

signals:
    // Fired once, right before a gesture's *first* Flame::zoom/center
    // mutation - a single wheel notch's, or a whole drag's (emitted at
    // mousePressEvent, before the first mouseMoveEvent mutates anything).
    // Exists specifically so a caller wanting to snapshot pre-gesture state
    // for its own undo history (MainWindow's A4) has a point to do that
    // BEFORE any mutation - unlike cameraChanged() below, which fires
    // *after* wheelEvent/mouseMoveEvent have already mutated Flame::zoom/
    // center for that step, too late to capture a true "before" snapshot.
    void gestureStarted();
    // Fired whenever a gesture changes Flame::zoom/center - continuously
    // while dragging (coalesce on the caller's side, same contract as
    // TriangleCanvas::xformEdited firing on every mouse-move during a
    // drag).
    void cameraChanged();
    // Fired once a gesture is "settled" - a drag's mouse release, or
    // (immediately) after a single wheel-zoom step, since one notch is
    // already a complete, discrete gesture rather than a continuous one.
    // The caller is expected to treat this as the point to request a
    // full-quality re-render, having used cameraChanged() for cheap
    // preview-quality feedback during anything continuous.
    void cameraChangeFinished();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    // The on-screen rect the current pixmap is actually drawn in - Qt
    // centers a pixmap smaller than the label (Qt::AlignCenter, set in the
    // constructor) rather than stretching it, so a mouse position has to
    // be re-based against this rect, not the label's own (0,0) origin,
    // before it means anything in image-pixel space.
    QRect displayedPixmapRect() const;
    // Converts a position in this widget's own local coordinates into a
    // point in the flame's own coordinate space, via displayedPixmapRect()
    // (widget -> rendered-image pixel space) then the same corner/
    // pixels-per-unit projection Renderer.cpp's buildBuffersAndCamera()
    // uses (rendered-image pixel space -> flame space), inverted.
    QPointF widgetToFlame(QPointF widgetPos) const;

    std::shared_ptr<apo::Flame> flame_;
    QSize renderedImageSize_{0, 0};

    bool dragging_ = false;
    QPointF lastDragPos_;
};

} // namespace apo::ui
