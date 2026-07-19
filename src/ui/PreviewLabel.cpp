#include "PreviewLabel.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QWheelEvent>

namespace apo::ui {

namespace {
// One wheel "notch" is 120 units of QWheelEvent::angleDelta() by Qt
// convention; each notch nudges Flame::zoom (a power-of-2 exponent - see
// Renderer.cpp's buildBuffersAndCamera: effective pixels-per-unit is
// pixelsPerUnit * 2^zoom) by this much.
constexpr double kZoomStepPerNotch = 0.15;
} // namespace

PreviewLabel::PreviewLabel(QWidget* parent) : QLabel(parent) {}

void PreviewLabel::setFlame(std::shared_ptr<apo::Flame> flame) { flame_ = std::move(flame); }

void PreviewLabel::setRenderedImageSize(QSize size) { renderedImageSize_ = size; }

QRect PreviewLabel::displayedPixmapRect() const {
    const QPixmap pm = pixmap();
    if (pm.isNull()) return rect();
    QRect r(QPoint(0, 0), pm.size());
    r.moveCenter(rect().center());
    return r;
}

QPointF PreviewLabel::widgetToFlame(QPointF widgetPos) const {
    const QRect displayRect = displayedPixmapRect();
    const double scaleFactor = renderedImageSize_.width() > 0
                                    ? static_cast<double>(displayRect.width()) / renderedImageSize_.width()
                                    : 1.0;
    if (!flame_ || scaleFactor <= 0.0) return QPointF(0, 0);

    // Widget pixel -> rendered-image pixel (column/row).
    const double imageC = (widgetPos.x() - displayRect.left()) / scaleFactor;
    const double imageR = (widgetPos.y() - displayRect.top()) / scaleFactor;

    // Rendered-image pixel -> flame space, inverting buildBuffersAndCamera's
    // cornerX/cornerY: column c maps to center.x + (c - width/2)/ppux
    // (increasing rightward, same direction as flame-space X); row r maps
    // to center.y - (r - height/2)/ppuy (the y-flip - image rows grow
    // downward, flame-space Y grows upward, same convention
    // TriangleCanvas::flameToWidget/widgetToFlame document).
    const double scale = std::pow(2.0, flame_->zoom);
    const double ppux = flame_->pixelsPerUnit * scale;
    const double ppuy = flame_->pixelsPerUnit * scale;
    const double flameX = flame_->center[0] + (imageC - renderedImageSize_.width() / 2.0) / ppux;
    const double flameY = flame_->center[1] - (imageR - renderedImageSize_.height() / 2.0) / ppuy;
    return QPointF(flameX, flameY);
}

void PreviewLabel::wheelEvent(QWheelEvent* event) {
    if (!flame_ || renderedImageSize_.isEmpty()) {
        QLabel::wheelEvent(event);
        return;
    }

    const double steps = event->angleDelta().y() / 120.0;
    if (steps == 0.0) {
        event->ignore();
        return;
    }

    emit gestureStarted(); // before any mutation below - see the signal's own doc comment
    const QPointF cursorFlame = widgetToFlame(event->position());
    const double oldScale = std::pow(2.0, flame_->zoom);
    flame_->zoom += steps * kZoomStepPerNotch;
    const double newScale = std::pow(2.0, flame_->zoom);
    const double factor = newScale / oldScale; // >1 = zoomed in

    // Keep the flame-space point under the cursor fixed on screen:
    // newCenter = cursor + (oldCenter - cursor) / factor.
    flame_->center[0] = cursorFlame.x() + (flame_->center[0] - cursorFlame.x()) / factor;
    flame_->center[1] = cursorFlame.y() + (flame_->center[1] - cursorFlame.y()) / factor;

    // One wheel notch is already a complete, discrete gesture (unlike a
    // drag) - go straight to "settled" so it converges directly to a
    // full-quality render (see PreviewLabel.h's doc comment); rapid
    // scrolling still stays responsive because the caller is expected to
    // coalesce bursts of this signal, same as everywhere else in this
    // port that re-renders on every small UI event.
    emit cameraChanged();
    emit cameraChangeFinished();
    event->accept();
}

void PreviewLabel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && flame_) {
        dragging_ = true;
        lastDragPos_ = event->position();
        emit gestureStarted(); // before the drag's first mouseMoveEvent mutation - see the signal's own doc comment
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void PreviewLabel::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_ || !flame_ || renderedImageSize_.isEmpty()) {
        QLabel::mouseMoveEvent(event);
        return;
    }

    const QPointF delta = event->position() - lastDragPos_;
    lastDragPos_ = event->position();

    const QRect displayRect = displayedPixmapRect();
    const double scaleFactor = renderedImageSize_.width() > 0
                                    ? static_cast<double>(displayRect.width()) / renderedImageSize_.width()
                                    : 1.0;
    if (scaleFactor > 0.0) {
        // Widget-pixel drag delta -> rendered-image-pixel delta -> flame-
        // space delta, then the "grab and drag" sign convention (content
        // follows the cursor, so the camera center moves opposite to a
        // rightward/downward drag in X, but *with* it in Y, matching the
        // same corner-projection y-flip widgetToFlame() uses) - derived
        // directly from inverting buildBuffersAndCamera's projection, not
        // guessed: dragging right by dc image-pixels requires
        // center.x -= dc/ppux for the same flame-space point to still sit
        // under the cursor afterward; dragging down by dr requires
        // center.y += dr/ppuy for the same reason.
        const double dc = delta.x() / scaleFactor;
        const double dr = delta.y() / scaleFactor;
        const double scale = std::pow(2.0, flame_->zoom);
        const double ppux = flame_->pixelsPerUnit * scale;
        const double ppuy = flame_->pixelsPerUnit * scale;
        flame_->center[0] -= dc / ppux;
        flame_->center[1] += dr / ppuy;
        emit cameraChanged();
    }
    event->accept();
}

void PreviewLabel::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        emit cameraChangeFinished();
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}

} // namespace apo::ui
