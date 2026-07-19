#include "CurvesWidget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <QMouseEvent>
#include <QPainter>

namespace apo::ui {

namespace {
constexpr double kPointHitRadius = 8.0; // matches CurvesControl.pas's point_size

// Matches PaintCurve's exact r[]/g[]/b[] tables (verified directly against
// source) - a dim tone for every non-active channel, a bright tone for
// whichever channel is currently being edited, so all 4 curves stay
// visible at once without the inactive ones competing for attention.
QColor channelColor(int channel, bool active) {
    static constexpr int kRDim[4] = {0xaa, 0xaa, 0x40, 0x40};
    static constexpr int kGDim[4] = {0xaa, 0x40, 0xaa, 0x40};
    static constexpr int kBDim[4] = {0xaa, 0x40, 0x40, 0xaa};
    static constexpr int kRBright[4] = {0xff, 0xff, 0x80, 0x80};
    static constexpr int kGBright[4] = {0xff, 0x80, 0xff, 0x80};
    static constexpr int kBBright[4] = {0xff, 0x80, 0x80, 0xff};
    if (active) return QColor(kRBright[channel], kGBright[channel], kBBright[channel]);
    return QColor(kRDim[channel], kGDim[channel], kBDim[channel]);
}
} // namespace

CurvesWidget::CurvesWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(200, 150);
    setMouseTracking(false);
}

void CurvesWidget::setCurves(const std::array<apo::BezierCurve, 4>& curves) {
    curves_ = curves;
    update();
}

void CurvesWidget::setActiveChannel(int channel) {
    activeChannel_ = std::clamp(channel, 0, 3);
    update();
}

void CurvesWidget::setActiveChannelWeight(int pointIndex, double weight) {
    curves_[static_cast<size_t>(activeChannel_)].weights[static_cast<size_t>(pointIndex)] = weight;
    update();
    emit curveChanged();
}

QPointF CurvesWidget::curveToWidget(double x, double y) const {
    // Y is flipped: curve y=1 (brightest) draws at the top, matching
    // BezierSetRect(flip=true) exactly - the natural "up is brighter"
    // convention for a tone curve.
    const double w = std::max(1, width() - 1);
    const double h = std::max(1, height() - 1);
    return {x * w, (1.0 - y) * h};
}

QPointF CurvesWidget::widgetToCurve(const QPointF& widgetPt) const {
    const double w = std::max(1, width() - 1);
    const double h = std::max(1, height() - 1);
    const double x = std::clamp(widgetPt.x() / w, 0.0, 1.0);
    const double y = std::clamp(1.0 - widgetPt.y() / h, 0.0, 1.0);
    return {x, y};
}

int CurvesWidget::hitTestControlPoint(const QPointF& widgetPos) const {
    const auto& curve = curves_[static_cast<size_t>(activeChannel_)];
    for (int i : {1, 2}) {
        const QPointF p = curveToWidget(curve.points[static_cast<size_t>(i)][0], curve.points[static_cast<size_t>(i)][1]);
        if (std::abs(widgetPos.x() - p.x()) <= kPointHitRadius && std::abs(widgetPos.y() - p.y()) <= kPointHitRadius) {
            return i;
        }
    }
    return -1;
}

void CurvesWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    // Grid - 8 columns, 4 rows, matching FramePaint's exact division counts.
    painter.setPen(QColor(0x55, 0x55, 0x55));
    for (int x = 1; x <= 7; ++x) {
        const int px = static_cast<int>(std::lround(0.125 * x * (width() - 1)));
        painter.drawLine(px, 0, px, height() - 1);
    }
    for (int y = 1; y <= 3; ++y) {
        const int py = static_cast<int>(std::lround(0.25 * y * (height() - 1)));
        painter.drawLine(0, py, width() - 1, py);
    }

    // Every inactive channel first (dim, drawn under), then the active one
    // last (bright, drawn on top) - matches FramePaint's own draw order.
    for (int c = 0; c < 4; ++c) {
        if (c == activeChannel_) continue;
        painter.setPen(channelColor(c, false));
        QPointF prev = curveToWidget(0.0, apo::evalBezierCurve(curves_[static_cast<size_t>(c)], 0.0));
        for (int i = 1; i <= 200; ++i) {
            const double t = i / 200.0;
            const auto pt = apo::solveBezierCurve(curves_[static_cast<size_t>(c)], t);
            const QPointF cur = curveToWidget(pt[0], pt[1]);
            painter.drawLine(prev, cur);
            prev = cur;
        }
    }

    const auto& active = curves_[static_cast<size_t>(activeChannel_)];
    const QPointF p1 = curveToWidget(active.points[1][0], active.points[1][1]);
    const QPointF p2 = curveToWidget(active.points[2][0], active.points[2][1]);

    // Handle guide lines (bottom-left corner -> P1, P1 -> P2, P2 -> top-
    // right corner) - matches PaintCurve's `widgets` guide-line drawing.
    painter.setPen(QColor(0x80, 0x80, 0x80));
    painter.drawLine(curveToWidget(0.0, 0.0), p1);
    painter.drawLine(p1, p2);
    painter.drawLine(p2, curveToWidget(1.0, 1.0));

    painter.setPen(channelColor(activeChannel_, true));
    QPointF prev = curveToWidget(0.0, apo::evalBezierCurve(active, 0.0));
    for (int i = 1; i <= 200; ++i) {
        const double t = i / 200.0;
        const auto pt = apo::solveBezierCurve(active, t);
        const QPointF cur = curveToWidget(pt[0], pt[1]);
        painter.drawLine(prev, cur);
        prev = cur;
    }

    painter.setBrush(channelColor(activeChannel_, true));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(p1, kPointHitRadius / 2.0, kPointHitRadius / 2.0);
    painter.drawEllipse(p2, kPointHitRadius / 2.0, kPointHitRadius / 2.0);
}

void CurvesWidget::mousePressEvent(QMouseEvent* event) {
    dragPointIndex_ = hitTestControlPoint(event->position());
}

void CurvesWidget::mouseMoveEvent(QMouseEvent* event) {
    if (dragPointIndex_ < 0) return;

    auto& curve = curves_[static_cast<size_t>(activeChannel_)];
    const QPointF c = widgetToCurve(event->position());
    curve.points[static_cast<size_t>(dragPointIndex_)] = {c.x(), c.y()};

    // If dragging made P1 cross past P2 (or vice versa), swap them and
    // keep tracking whichever index now holds the point under the cursor -
    // matches FrameMouseMove's identical swap-and-retarget behavior
    // exactly (without it, a fast drag past the other handle would start
    // moving the wrong point).
    if (curve.points[1][0] > curve.points[2][0]) {
        std::swap(curve.points[1], curve.points[2]);
        dragPointIndex_ = (dragPointIndex_ == 1) ? 2 : 1;
    }

    update();
    emit curveChanged();
}

void CurvesWidget::mouseReleaseEvent(QMouseEvent* /*event*/) {
    dragPointIndex_ = -1;
}

void CurvesWidget::leaveEvent(QEvent* /*event*/) {
    // Matches FrameMouseLeave calling FrameMouseUp - a drag that carries
    // the cursor off the widget entirely still ends the gesture cleanly.
    dragPointIndex_ = -1;
}

} // namespace apo::ui
