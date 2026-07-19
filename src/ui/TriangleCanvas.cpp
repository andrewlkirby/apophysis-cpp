#include "TriangleCanvas.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace apo::ui {

namespace {
constexpr double kVertexHitRadiusPx = 9.0;
constexpr double kSnapAngleDegrees = 15.0;
constexpr double kSnapScaleStep = 0.1;
// How close (in widget pixels) a click has to land to the previous one to
// count as "the same spot" for overlap-cycling purposes - loose enough to
// tolerate real hand-tremor between clicks, tight enough not to treat two
// deliberately different clicks as a continuation.
constexpr double kClickCycleTolerancePx = 4.0;
// Not <cmath>'s M_PI: MSVC only defines that behind _USE_MATH_DEFINES (see
// e.g. Rng.h's kTwoPi for the same reasoning elsewhere in this codebase).
constexpr double kPi = 3.14159265358979323846;

// One distinct hue per unselected xform (cycled via modulo past this many),
// so overlapping triangles - common once a flame has more than 2-3 xforms -
// stay distinguishable instead of blurring into one uniform dotted mass.
// Deliberately excludes yellow (reserved for the selected triangle) - the
// pivot marker is drawn white/black-outlined instead of a reserved color,
// so it stays visible against any of these.
const QColor kXformPalette[] = {
    QColor(120, 160, 220), QColor(220, 120, 160), QColor(140, 220, 140), QColor(220, 150, 90),
    QColor(180, 120, 220), QColor(100, 200, 180), QColor(120, 200, 230), QColor(220, 120, 220),
};

} // namespace

QColor TriangleCanvas::xformColor(int index) {
    return kXformPalette[static_cast<size_t>(index) % std::size(kXformPalette)];
}

TriangleCanvas::TriangleCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(false);
    setMinimumSize(200, 200);
    setFocusPolicy(Qt::StrongFocus);
}

void TriangleCanvas::setFlame(std::shared_ptr<apo::Flame> flame) {
    flame_ = std::move(flame);
    selectedXform_ = flame_ && flame_->numXForms() > 0 ? 0 : -1;
    fitView();
    update();
}

void TriangleCanvas::setSelectedXform(int index) {
    if (!flame_) return;
    if (index == apo::kFinalXformIndex) {
        if (!flame_->finalXformEnabled) return;
        selectedXform_ = index;
        update();
        return;
    }
    if (index < 0 || index >= flame_->numXForms()) return;
    selectedXform_ = index;
    update();
}

void TriangleCanvas::setPivotPickArmed(bool armed) {
    pivotPickArmed_ = armed;
}

void TriangleCanvas::clearCustomPivot() {
    hasCustomPivot_ = false;
    update();
}

void TriangleCanvas::setEditingPostTransform(bool post) {
    editPost_ = post;
    update();
}

void TriangleCanvas::setBackgroundImage(const QImage& image) {
    background_ = image;
    update();
}

apo::Triangle TriangleCanvas::triangleFor(int xformIndex) const {
    const apo::XForm& xf = (xformIndex == apo::kFinalXformIndex) ? *flame_->finalXform : *flame_->xform[xformIndex];
    return apo::triangleFromCoefs(editPost_ ? xf.p : xf.c);
}

void TriangleCanvas::writeTriangle(int xformIndex, const apo::Triangle& t) {
    apo::XForm& xf = (xformIndex == apo::kFinalXformIndex) ? *flame_->finalXform : *flame_->xform[xformIndex];
    auto coefs = apo::coefsFromTriangle(t);
    if (editPost_) {
        xf.p = coefs;
    } else {
        xf.c = coefs;
    }
}

void TriangleCanvas::fitView() {
    if (!flame_ || flame_->numXForms() == 0) {
        viewCenter_ = {0, 0};
        viewScale_ = 80.0;
        update();
        return;
    }

    double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
    auto accumulate = [&](const apo::Triangle& t) {
        for (apo::Point2 p : {t.o, t.x, t.y}) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
    };
    for (int i = 0; i < flame_->numXForms(); ++i) accumulate(triangleFor(i));
    if (flame_->finalXformEnabled) accumulate(triangleFor(apo::kFinalXformIndex));

    viewCenter_ = {(minX + maxX) / 2.0, (minY + maxY) / 2.0};

    const double spanX = std::max(0.1, maxX - minX);
    const double spanY = std::max(0.1, maxY - minY);
    const double margin = 1.4; // leave headroom around the fitted bounds
    const double scaleX = width() / (spanX * margin);
    const double scaleY = height() / (spanY * margin);
    viewScale_ = std::clamp(std::min(scaleX, scaleY), 5.0, 2000.0);
    update();
}

QPointF TriangleCanvas::flameToWidget(apo::Point2 p) const {
    // Flame-space Y grows upward (matches the affine-coefficient
    // convention throughout this port); widget/screen Y grows downward -
    // flipped here, once, rather than baking a sign flip into the
    // triangle math itself.
    return QPointF(width() / 2.0 + (p.x - viewCenter_.x) * viewScale_,
                    height() / 2.0 - (p.y - viewCenter_.y) * viewScale_);
}

apo::Point2 TriangleCanvas::widgetToFlame(QPointF p) const {
    return {viewCenter_.x + (p.x() - width() / 2.0) / viewScale_,
            viewCenter_.y - (p.y() - height() / 2.0) / viewScale_};
}

void TriangleCanvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(20, 20, 24));

    if (!background_.isNull()) {
        painter.drawImage(rect(), background_, background_.rect());
    }

    // Origin crosshair - a fixed reference point independent of any
    // xform, so the canvas still orients you when nothing is selected.
    painter.setPen(QPen(QColor(255, 255, 255, 40), 1));
    const QPointF originPx = flameToWidget({0, 0});
    painter.drawLine(QPointF(originPx.x(), 0), QPointF(originPx.x(), height()));
    painter.drawLine(QPointF(0, originPx.y()), QPointF(width(), originPx.y()));

    if (!flame_) return;

    // Shared by every per-xform triangle (regular or final): the polygon
    // itself, plus - only when selected - the dimmed opposite-mode
    // reference triangle and the O/X/Y vertex markers. Factored out of the
    // old per-xform-only loop below so the final transform (drawn
    // separately afterward, since it isn't part of flame_->xform[]) gets
    // exactly the same treatment instead of a parallel, easy-to-drift copy.
    auto drawTriangle = [&](const apo::Triangle& t, bool selected, QColor unselectedColor,
                             Qt::PenStyle unselectedStyle, const apo::XForm& xf) {
        const QPointF po = flameToWidget(t.o);
        const QPointF px = flameToWidget(t.x);
        const QPointF py = flameToWidget(t.y);

        QColor color = selected ? QColor(255, 210, 60) : unselectedColor;
        QPen pen(color, selected ? 2.0 : 1.0);
        if (!selected) pen.setStyle(unselectedStyle);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(QPolygonF({po, px, py}));

        if (selected) {
            // Reference (non-interactive) opposite-mode triangle, dimmed -
            // lets you see roughly where the post-transform sits while
            // editing the main one, or vice versa, without it being
            // draggable (see setEditingPostTransform's doc comment).
            const apo::Triangle other = apo::triangleFromCoefs(editPost_ ? xf.c : xf.p);
            const bool otherIsIdentity = other.o.x == 0 && other.o.y == 0 && other.x.x == 1 && other.x.y == 0 &&
                                          other.y.x == 0 && other.y.y == 1;
            if (!otherIsIdentity) {
                QPen refPen(QColor(150, 150, 150, 90), 1, Qt::DashLine);
                painter.setPen(refPen);
                painter.drawPolygon(QPolygonF({flameToWidget(other.o), flameToWidget(other.x), flameToWidget(other.y)}));
            }

            painter.setPen(QPen(color, 2));
            painter.setBrush(color);
            painter.drawEllipse(po, 4, 4);
            painter.drawEllipse(px, 3, 3);
            painter.drawEllipse(py, 3, 3);
            painter.setFont(QFont(painter.font().family(), 8));
            painter.drawText(po + QPointF(6, -6), "O");
            painter.drawText(px + QPointF(6, -6), "X");
            painter.drawText(py + QPointF(6, -6), "Y");
        }
    };

    for (int i = 0; i < flame_->numXForms(); ++i) {
        drawTriangle(triangleFor(i), i == selectedXform_, xformColor(i), Qt::DotLine, *flame_->xform[i]);
    }
    if (flame_->finalXformEnabled) {
        // White, dash-dot outline when unselected - deliberately outside
        // the numbered per-xform palette (kXformPalette), so the final
        // transform reads as categorically different from a regular one,
        // matching the original's own distinct-color treatment of it.
        drawTriangle(triangleFor(apo::kFinalXformIndex), selectedXform_ == apo::kFinalXformIndex,
                     QColor(255, 255, 255), Qt::DashDotLine, *flame_->finalXform);
    }

    // Drawn as a black-then-white double-stroked crosshair (not one of
    // the reserved palette colors) so it stays visible regardless of
    // which xform colors or background pixels sit underneath it.
    // Pivot-pick-armed-but-not-yet-set has no on-canvas indicator - this
    // widget doesn't track mouse position outside an active drag, so
    // there's no cursor-following hint to draw; EditorWindow's status bar
    // is what tells the user pick mode is active.
    if (hasCustomPivot_) {
        const QPointF pivotPx = flameToWidget(customPivot_);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::black, 3));
        painter.drawLine(pivotPx + QPointF(-7, 0), pivotPx + QPointF(7, 0));
        painter.drawLine(pivotPx + QPointF(0, -7), pivotPx + QPointF(0, 7));
        painter.setPen(QPen(Qt::white, 1.5));
        painter.drawLine(pivotPx + QPointF(-7, 0), pivotPx + QPointF(7, 0));
        painter.drawLine(pivotPx + QPointF(0, -7), pivotPx + QPointF(0, 7));
        painter.drawEllipse(pivotPx, 3, 3);
    }
}

TriangleCanvas::HitResult TriangleCanvas::hitTestVerticesOnly(int xformIndex, QPointF widgetPos) const {
    const apo::Triangle t = triangleFor(xformIndex);
    const QPointF po = flameToWidget(t.o), px = flameToWidget(t.x), py = flameToWidget(t.y);

    struct Candidate {
        double dist;
        HitTarget target;
    };
    const Candidate candidates[3] = {
        {QLineF(widgetPos, po).length(), HitTarget::VertexO},
        {QLineF(widgetPos, px).length(), HitTarget::VertexX},
        {QLineF(widgetPos, py).length(), HitTarget::VertexY},
    };
    const Candidate* best = &candidates[0];
    for (const auto& c : candidates) {
        if (c.dist < best->dist) best = &c;
    }
    if (best->dist <= kVertexHitRadiusPx) return {xformIndex, best->target};
    return {-1, HitTarget::None};
}

TriangleCanvas::HitResult TriangleCanvas::hitTestFull(int xformIndex, QPointF widgetPos) const {
    HitResult vertexHit = hitTestVerticesOnly(xformIndex, widgetPos);
    if (vertexHit.target != HitTarget::None) return vertexHit;

    const apo::Point2 flamePos = widgetToFlame(widgetPos);
    const apo::Triangle t = triangleFor(xformIndex);
    if (apo::pointInTriangle(flamePos, t)) return {xformIndex, HitTarget::Body};
    return {-1, HitTarget::None};
}

std::vector<TriangleCanvas::HitResult> TriangleCanvas::hitTestAllAt(QPointF widgetPos) const {
    std::vector<HitResult> hits;
    for (int i = 0; i < flame_->numXForms(); ++i) {
        const HitResult hit = hitTestFull(i, widgetPos);
        if (hit.target != HitTarget::None) hits.push_back(hit);
    }
    if (flame_->finalXformEnabled) {
        const HitResult hit = hitTestFull(apo::kFinalXformIndex, widgetPos);
        if (hit.target != HitTarget::None) hits.push_back(hit);
    }
    return hits;
}

void TriangleCanvas::beginDrag(const HitResult& hit, QPointF widgetPos) {
    dragging_ = true;
    dragTarget_ = hit;
    dragStartTriangle_ = triangleFor(hit.xformIndex);
    dragPivot_ = hasCustomPivot_ ? customPivot_ : apo::centroid(dragStartTriangle_);
    mouseDownFlamePos_ = widgetToFlame(widgetPos);
    dragStartWidgetPos_ = widgetPos;
    dragExceededClickTolerance_ = false;
    emit editingStarted(hit.xformIndex);
}

void TriangleCanvas::mousePressEvent(QMouseEvent* event) {
    if (!flame_ || flame_->numXForms() == 0 || event->button() != Qt::LeftButton) return;

    if (pivotPickArmed_) {
        customPivot_ = widgetToFlame(event->pos());
        hasCustomPivot_ = true;
        pivotPickArmed_ = false;
        update();
        return;
    }

    const std::vector<HitResult> hits = hitTestAllAt(event->pos());
    if (hits.empty()) {
        clickCycleActive_ = false;
        return;
    }

    std::vector<int> candidates;
    candidates.reserve(hits.size());
    for (const HitResult& h : hits) candidates.push_back(h.xformIndex);

    // Overlap-cycling: a click landing on more than one triangle (shared
    // vertex, or overlapping bodies) picks the first candidate the first
    // time, then advances to the next one on each subsequent click at
    // (about) the same spot with the same candidates under it - cycling
    // all the way through before wrapping back to the first. A single
    // unambiguous hit always just selects/drags that one triangle, same
    // as before this existed.
    int index = 0;
    if (clickCycleActive_ && candidates == lastClickCandidates_ &&
        QLineF(event->pos(), lastClickWidgetPos_).length() <= kClickCycleTolerancePx) {
        index = (lastClickCycleIndex_ + 1) % static_cast<int>(hits.size());
    }

    lastClickWidgetPos_ = event->pos();
    lastClickCandidates_ = candidates;
    lastClickCycleIndex_ = index;
    clickCycleActive_ = true;

    const HitResult& hit = hits[static_cast<size_t>(index)];
    if (hit.xformIndex != selectedXform_) {
        selectedXform_ = hit.xformIndex;
        emit selectedXformChanged(hit.xformIndex);
        update();
    }
    beginDrag(hit, event->pos());
}

void TriangleCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) return;

    if (QLineF(event->pos(), dragStartWidgetPos_).length() > kClickCycleTolerancePx) {
        dragExceededClickTolerance_ = true;
    }

    const apo::Point2 mouseFlame = widgetToFlame(event->pos());
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    // Holding Ctrl scales the dragged triangle regardless of the current
    // Move/Rotate/Scale toolbar mode - a dedicated resize gesture so you
    // don't have to switch modes just to nudge a triangle's size (see the
    // class comment's note on this being a deliberate departure from the
    // original, which reaches scale from any mode via its per-corner
    // "widget" hit-zones instead - not ported here).
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    const EditMode effectiveMode = ctrl ? EditMode::Scale : mode_;

    // Grabbing the Origin with Shift held translates the whole triangle
    // rather than shearing it (preserves the O->X/O->Y vectors) - the one
    // deliberately-preserved special case from the original's axis-lock
    // behavior (see TriangleGeometry.h's file comment for what else was
    // simplified away).
    const bool wholeTriangle =
        dragTarget_.target == HitTarget::Body ||
        (dragTarget_.target == HitTarget::VertexO && shift && effectiveMode == EditMode::Move);

    apo::Triangle result = dragStartTriangle_;

    if (effectiveMode == EditMode::Move) {
        apo::Point2 delta{mouseFlame.x - mouseDownFlamePos_.x, mouseFlame.y - mouseDownFlamePos_.y};
        if (shift && !wholeTriangle) {
            // Axis-lock: zero out whichever component is smaller, so the
            // drag is constrained to a single axis.
            if (std::fabs(delta.x) < std::fabs(delta.y)) {
                delta.x = 0;
            } else {
                delta.y = 0;
            }
        }
        auto move = [&](apo::Point2 p) { return apo::Point2{p.x + delta.x, p.y + delta.y}; };
        if (wholeTriangle) {
            result.o = move(dragStartTriangle_.o);
            result.x = move(dragStartTriangle_.x);
            result.y = move(dragStartTriangle_.y);
        } else if (dragTarget_.target == HitTarget::VertexO) {
            result.o = move(dragStartTriangle_.o);
        } else if (dragTarget_.target == HitTarget::VertexX) {
            result.x = move(dragStartTriangle_.x);
        } else {
            result.y = move(dragStartTriangle_.y);
        }
    } else if (effectiveMode == EditMode::Rotate) {
        double angle = apo::angleBetween(dragPivot_, mouseDownFlamePos_, mouseFlame);
        if (shift) {
            const double stepRad = kSnapAngleDegrees * kPi / 180.0;
            angle = std::round(angle / stepRad) * stepRad;
        }
        auto rot = [&](apo::Point2 p) { return apo::rotateAroundPivot(p, dragPivot_, angle); };
        if (wholeTriangle) {
            result.o = rot(dragStartTriangle_.o);
            result.x = rot(dragStartTriangle_.x);
            result.y = rot(dragStartTriangle_.y);
        } else if (dragTarget_.target == HitTarget::VertexO) {
            result.o = rot(dragStartTriangle_.o);
        } else if (dragTarget_.target == HitTarget::VertexX) {
            result.x = rot(dragStartTriangle_.x);
        } else {
            result.y = rot(dragStartTriangle_.y);
        }
    } else { // Scale
        double factor = apo::scaleFactorBetween(dragPivot_, mouseDownFlamePos_, mouseFlame);
        if (shift) factor = std::round(factor / kSnapScaleStep) * kSnapScaleStep;
        auto scl = [&](apo::Point2 p) { return apo::scaleAroundPivot(p, dragPivot_, factor); };
        if (wholeTriangle) {
            result.o = scl(dragStartTriangle_.o);
            result.x = scl(dragStartTriangle_.x);
            result.y = scl(dragStartTriangle_.y);
        } else if (dragTarget_.target == HitTarget::VertexO) {
            result.o = scl(dragStartTriangle_.o);
        } else if (dragTarget_.target == HitTarget::VertexX) {
            result.x = scl(dragStartTriangle_.x);
        } else {
            result.y = scl(dragStartTriangle_.y);
        }
    }

    writeTriangle(dragTarget_.xformIndex, result);
    update();
    emit xformEdited(dragTarget_.xformIndex);
}

void TriangleCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !dragging_) return;
    dragging_ = false;
    // A real drag (the pointer actually moved, even if it ends up back
    // near its start - see dragExceededClickTolerance_), as opposed to a
    // plain click that happened to also hit a draggable target, shouldn't
    // feed the overlap-cycling sequence - the next click, even at this
    // same spot, should start back at the first candidate rather than
    // continuing wherever a drag left off.
    if (dragExceededClickTolerance_) {
        clickCycleActive_ = false;
    }
    emit editingFinished(dragTarget_.xformIndex);
}

void TriangleCanvas::wheelEvent(QWheelEvent* event) {
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    viewScale_ = std::clamp(viewScale_ * factor, 5.0, 2000.0);
    update();
    event->accept();
}

void TriangleCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

} // namespace apo::ui
