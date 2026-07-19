#pragma once

#include <array>

#include <QWidget>

#include "core/Bezier.h"

class QMouseEvent;
class QPaintEvent;

namespace apo::ui {

// The draggable Bezier tone-curve graph - port of System/CurvesControl.pas's
// custom-drawn TFrame (its FramePaint/FrameMouseDown/Move/Up handlers).
// Draws all 4 channels (dimmed) with the active one highlighted on top,
// complete with its two draggable control-point handles (P0/P3 are always
// fixed at (0,0)/(1,1) - only P1/P2 are ever draggable, matching the
// original's `for i := 1 to n-2` hit-test bound exactly).
class CurvesWidget final : public QWidget {
    Q_OBJECT

public:
    explicit CurvesWidget(QWidget* parent = nullptr);

    void setCurves(const std::array<apo::BezierCurve, 4>& curves);
    const std::array<apo::BezierCurve, 4>& curves() const { return curves_; }

    // 0=All(master), 1=Red, 2=Green, 3=Blue - matches Curves.pas's
    // TCurvesChannel ordering.
    void setActiveChannel(int channel);
    int activeChannel() const { return activeChannel_; }

    // Sets just the active channel's P1/P2 weight without touching point
    // positions or emitting a drag-style event loop - for the dialog's
    // weight sliders, which edit the curve by a different control than
    // dragging.
    void setActiveChannelWeight(int pointIndex, double weight);

signals:
    // Fired continuously while dragging a control point, and once after
    // setActiveChannelWeight() - live-preview should hook this the same
    // way AdjustDialog's SliderSpin::valueChanged is used.
    void curveChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QPointF curveToWidget(double x, double y) const;
    QPointF widgetToCurve(const QPointF& widgetPt) const;
    int hitTestControlPoint(const QPointF& widgetPos) const;

    std::array<apo::BezierCurve, 4> curves_;
    int activeChannel_ = 0;
    int dragPointIndex_ = -1; // 1 or 2 while dragging, else -1
};

} // namespace apo::ui
