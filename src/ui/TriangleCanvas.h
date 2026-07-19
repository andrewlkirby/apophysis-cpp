#pragma once

#include <memory>
#include <vector>

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QWidget>

#include "core/Flame.h"
#include "core/edit/TriangleGeometry.h"

namespace apo::ui {

// The Editor window's transform-triangle canvas - one draggable triangle
// per xform, overlaid on a (caller-supplied) preview render. This is the
// widget the migration plan calls "the single biggest UI risk" for Phase
// 6. See src/core/edit/TriangleGeometry.h's file comment for the overall
// approach: vertex *semantics* (what a triangle's O/X/Y positions mean)
// are preserved exactly from the original (verified against
// ControlPoint.pas), but the *interaction model* is a deliberately
// simplified from-scratch design, not a port of Editor.pas's TriangleView
// - specifically:
//   - One global edit mode (Move/Rotate/Scale, toolbar-selected) rather
//     than the original's context-sensitive edge/corner/"widget"
//     hit-zones that silently switch mode depending on exactly where you
//     click. A vertex or the triangle's body is always draggable; which
//     transform that drag performs is whatever mode is currently active.
//     Fewer hidden modes, at the cost of the original's slightly faster
//     "grab the hypotenuse to scale without touching a toolbar" shortcut.
//   - Pivot defaults to the selected triangle's own centroid, recomputed
//     at the start of each drag, but can be overridden by arming pivot-
//     pick mode (setPivotPickArmed) and clicking anywhere on the canvas -
//     a simpler one-shot-click design than the original's separate
//     world/local pivot-mode toggle plus barycentric-coordinate pivot
//     storage, but covers the same real use case (rotate/scale around an
//     arbitrary point, not just the triangle's center).
//   - Shift is a drag modifier (axis-lock for Move, 15-degree snap for
//     Rotate, 10% snap for Scale) - matches what the research behind this
//     port actually found in Editor.pas (no Ctrl/Alt behavior there
//     either). Ctrl is a departure from that: holding it during a drag
//     performs a Scale regardless of the current global mode, so you can
//     resize a triangle without leaving Move/Rotate mode first - a gap
//     the original doesn't need to fill, since its per-corner "widget"
//     hit-zones (deliberately not ported - see above) already let you
//     reach scale from any mode by grabbing the right spot.
//   - Clicking a point where more than one triangle's vertex or body
//     overlaps (common once several xforms sit near each other) selects
//     the first one under the cursor, then cycles to the next on each
//     subsequent click at (about) the same spot, rather than always
//     landing on the same one - see hitTestAllAt()/mousePressEvent().
class TriangleCanvas final : public QWidget {
    Q_OBJECT

public:
    enum class EditMode { Move, Rotate, Scale };

    explicit TriangleCanvas(QWidget* parent = nullptr);

    void setFlame(std::shared_ptr<apo::Flame> flame);
    // `index` is 0..flame's numXForms()-1 for a regular xform, or
    // apo::kFinalXformIndex for the flame's final transform (a no-op if
    // Flame::finalXformEnabled is currently false - there's nothing to
    // select). Every triangle operation below (drawing, hit-testing,
    // dragging, fitView) already treats a normal index and
    // kFinalXformIndex identically via triangleFor()/writeTriangle()'s own
    // special-casing, so this is the only place that needs to know the
    // sentinel is special.
    void setSelectedXform(int index);
    int selectedXform() const { return selectedXform_; }

    // The identifying color used for xform `index`'s triangle outline
    // (cycled across a fixed palette) - exposed so other xform-related UI
    // (the Xaos editor) can reuse the exact same per-xform color coding,
    // matching Editor.pas's GetTriangleColor being shared between the
    // triangle view and the Xaos grid's row-color swatches.
    static QColor xformColor(int index);
    void setEditMode(EditMode mode) { mode_ = mode; }
    EditMode editMode() const { return mode_; }

    // Whether the triangle being edited reads/writes the post-transform
    // (XForm::p) instead of the main affine (XForm::c) - toggles which
    // matrix vertex drags feed into, matching XForm::postXswap's role.
    void setEditingPostTransform(bool post);
    bool editingPostTransform() const { return editPost_; }

    // The live background render, already produced by the caller's own
    // render pipeline (RenderWorker et al.) - this widget only displays
    // it, scaled to fill the canvas, and never triggers a render itself.
    void setBackgroundImage(const QImage& image);

    // Recenters/rescales the canvas's own view (independent of the
    // flame's render camera - see the class comment) to fit every xform's
    // triangle with some margin. Called automatically when a new flame is
    // set; exposed so a caller can offer a "reset view" action too.
    void fitView();

    // Arms/disarms pivot-pick mode: while armed, the *next* click on the
    // canvas sets a custom rotate/scale pivot (shown as a marker) instead
    // of performing a normal triangle drag, then disarms itself
    // automatically. Does not affect Move (which never uses a pivot).
    void setPivotPickArmed(bool armed);
    bool hasCustomPivot() const { return hasCustomPivot_; }
    // Reverts to the default (selected triangle's centroid) pivot.
    void clearCustomPivot();

    // Coordinate conversion between the canvas's own flame-space view and
    // widget pixels, and the currently-displayed triangle for a given
    // xform (whichever of c/p editingPostTransform() selects). Genuinely
    // useful public API - not just a test hook - for anything that needs
    // to place UI relative to a specific vertex (a future numeric-entry
    // panel; also used by tests/ui/triangle_canvas_interaction_test.cpp to
    // compute exact click targets instead of guessing pixel positions).
    QPointF flameToWidget(apo::Point2 p) const;
    apo::Point2 widgetToFlame(QPointF p) const;
    apo::Triangle currentTriangle(int xformIndex) const { return triangleFor(xformIndex); }

signals:
    // Emitted once, when a drag begins (mouse press on a vertex/body) -
    // the natural point for a caller to snapshot pre-drag state for undo.
    void editingStarted(int index);
    // Emitted whenever a drag changes the selected xform's coefficients -
    // the caller (EditorWindow) is expected to trigger a re-render on
    // this, coalescing if one's already in flight (dragging fires this on
    // every mouse-move).
    void xformEdited(int index);
    // Emitted once when a drag completes (mouse release) - paired with
    // editingStarted to bracket one undo-history entry (see
    // TriangleGeometry.h's note that the original's own undo integration
    // for this widget was ambiguous/possibly incomplete; "one step per
    // completed drag" is this port's deliberate choice, not a preserved
    // behavior).
    void editingFinished(int index);
    void selectedXformChanged(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class HitTarget { None, VertexO, VertexX, VertexY, Body };
    struct HitResult {
        int xformIndex = -1;
        HitTarget target = HitTarget::None;
    };

    apo::Triangle triangleFor(int xformIndex) const;
    void writeTriangle(int xformIndex, const apo::Triangle& t);
    HitResult hitTestVerticesOnly(int xformIndex, QPointF widgetPos) const;
    HitResult hitTestFull(int xformIndex, QPointF widgetPos) const;
    // Every xform (regular, ascending index, then the final transform if
    // enabled) whose vertex or body is under `widgetPos`, in that same
    // fixed order - the candidate list mousePressEvent's overlap-cycling
    // walks through. Order is independent of the current selection so
    // repeated clicks at one spot advance through a stable sequence
    // instead of drifting as the selection itself changes.
    std::vector<HitResult> hitTestAllAt(QPointF widgetPos) const;
    void beginDrag(const HitResult& hit, QPointF widgetPos);

    std::shared_ptr<apo::Flame> flame_;
    int selectedXform_ = -1;
    EditMode mode_ = EditMode::Move;
    bool editPost_ = false;
    QImage background_;

    // Canvas's own view camera: flame-space point shown at the widget's
    // center, and pixels-per-flame-unit scale. Independent of the flame's
    // render camera (Flame::center/pixelsPerUnit/zoom) - editing the
    // triangles shouldn't require the flame's own output framing to
    // happen to include them.
    apo::Point2 viewCenter_{0, 0};
    double viewScale_ = 80.0;

    bool dragging_ = false;
    HitResult dragTarget_;
    apo::Triangle dragStartTriangle_;
    apo::Point2 dragPivot_;
    apo::Point2 mouseDownFlamePos_;
    QPointF dragStartWidgetPos_;
    // Whether the pointer has moved past kClickCycleTolerancePx from
    // dragStartWidgetPos_ at any point during the current drag - tracked
    // continuously (not just press-vs-release displacement), so a drag
    // that moves away and then back to its starting pixel still counts as
    // a real drag rather than being indistinguishable from a stationary
    // click.
    bool dragExceededClickTolerance_ = false;

    // Overlap-click-cycling state: which candidates (by xform index, in
    // hitTestAllAt's fixed order) and which one of them was picked by the
    // most recent completed click, so the next click can tell whether
    // it's a continuation (same spot -> advance to the next candidate) or
    // a fresh click elsewhere (-> start back at the first one). Cleared
    // whenever a press turns into a real drag (see mouseReleaseEvent) -
    // only genuine clicks chain together.
    bool clickCycleActive_ = false;
    QPointF lastClickWidgetPos_;
    std::vector<int> lastClickCandidates_;
    int lastClickCycleIndex_ = 0;

    bool pivotPickArmed_ = false;
    bool hasCustomPivot_ = false;
    apo::Point2 customPivot_{0, 0};
};

} // namespace apo::ui
