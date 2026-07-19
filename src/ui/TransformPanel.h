#pragma once

#include <functional>
#include <memory>

#include <QWidget>

#include "core/Flame.h"
#include "core/edit/TriangleGeometry.h"

class QLineEdit;
class QDoubleSpinBox;
class QTabWidget;
class QTableWidget;
class QTableWidgetItem;
class QCheckBox;
class QPushButton;
class QLabel;

namespace apo::ui {

class SliderSpin;

// Port of Editor.pas's right-hand transform-properties panel (screenshot-
// verified against the original: Transform/Name/Weight header row, then a
// tabbed Triangle/Transform/Colors/Variations/Variables/Xaos area) - the
// single largest gap this port had: without it, the Editor could drag
// triangles and add/delete/duplicate/mutate transforms, but had no way to
// actually *author* one (pick which variations it uses, tune their
// weights/parameters, or set its color/opacity/weight). Every field here
// already exists in the core data model (XForm.h) and already round-trips
// through FlameIO - this widget is pure UI wiring on top of it.
//
// Built incrementally, tab by tab, matching the original's own tab order
// where practical:
//   - Variations: every registered variation's weight for the selected
//     transform, via XForm::numVariations()/variation()/setVariation() and
//     VariationRegistry's name table.
//   - Colors: color position (XForm::color, with a swatch preview against
//     the flame's own gradient), color speed (XForm::symmetry - the
//     ".flame" file's "color_speed" attribute, verified via FlameIO.cpp),
//     opacity (XForm::transOpacity), direct/variable color
//     (XForm::pluginColor - the "var_color" attribute), and a Solo toggle
//     (Flame::soloXform, a flame-level field - checking it here sets
//     soloXform to *this* xform's index, unchecking clears it to -1; the
//     renderer already honors it, see Renderer.cpp).
//   - Variables: every registered variation's own named parameters
//     (XForm::getVariable()/setVariable(), VariationRegistry's flattened
//     variable-name table) - shown only for variations with a nonzero
//     weight, matching the original's own behavior (a variable belonging
//     to a variation this transform doesn't use isn't meaningful to edit).
//   - Transform: numeric editing of the selected xform's triangle, folding
//     the original's separate "Transform" (raw a..f coefficients) and
//     "Triangle" (per-vertex X/Y) tabs into one vertex-based editor -
//     O/X/Y each get their own X/Y spin box pair, round-tripped through
//     TriangleGeometry.h's triangleFromCoefs()/coefsFromTriangle() (the
//     same pure math TriangleCanvas's own drag code uses), which is
//     strictly more information than raw a..f (a..f *is* just these same
//     six numbers in a less intuitive basis) - a deliberate "one coherent
//     editor, not two redundant ones" simplification. An "Edit post
//     transform" checkbox switches which of XForm::c/p the six fields (and
//     the rotate/scale/flip tools below) read and write, kept in sync
//     bidirectionally with EditorWindow's own toolbar "Post" toggle (which
//     drives TriangleCanvas::setEditingPostTransform) via
//     editingPostTransformChanged()/setEditingPostTransform() - so numeric
//     editing and canvas dragging always agree on which triangle is
//     "active". Also hosts the original's exact-value tools (rotate by N
//     degrees, scale by N percent, rotate 90 CW/CCW, flip horizontal/
//     vertical), implemented via the same TriangleGeometry helpers
//     (rotateAroundPivot/scaleAroundPivot) around the active triangle's own
//     centroid - matching the semantics of a canvas drag, just exact-value
//     instead of mouse-driven.
// Final-transform UI (enable toggle, canvas drawing/selection, transform-
// selector inclusion) and copy/paste-transform: still a follow-up pass
// (see MISSING_FEATURES_PLAN.txt's P1(c) progress log).
// Xaos is NOT folded in here - it stays its own dialog (XaosDialog), a
// deliberate difference from the original's tabbed layout: XaosDialog
// already shows the *entire* NxN weight matrix at once (see its own class
// comment), strictly more information than a per-transform row/column
// view would fit in this panel's tab space.
//
// Owns no undo/redraw policy itself - it directly mutates whatever Flame
// it's given (same "shared, in-place" pattern as Curves/Xaos/PostProcess)
// and emits three signals for the owning EditorWindow to act on:
// propertyEdited() (live, for re-rendering), editingStarted()/
// editingFinished() (bracket one undo-worthy gesture - a slider drag, a
// line edit session, or one committed table-cell edit).
class TransformPanel final : public QWidget {
    Q_OBJECT

public:
    explicit TransformPanel(QWidget* parent = nullptr);

    // Called once, when the owning EditorWindow's Flame is known (matches
    // TriangleCanvas::setFlame's contract - same Flame instance for the
    // panel's whole lifetime, only its *contents* change).
    void setFlame(std::shared_ptr<apo::Flame> flame);

    // Selects which xform's properties to display/edit (-1 = none - shows
    // disabled/blank fields; apo::kFinalXformIndex = the flame's final
    // transform, if enabled - every tab here works on it exactly like any
    // other xform, since currentXForm() resolves the sentinel once and
    // every tab is already written purely in terms of currentXForm()).
    // Always refreshes every displayed value from the Flame, whether or
    // not the index actually changed - the caller is expected to call this
    // after *any* external mutation that might have touched the
    // currently-selected xform (undo/redo, Add/Duplicate/Delete, Force
    // Symmetry), not just on an actual selection change.
    void setSelectedXform(int index);
    int selectedXform() const { return selectedIndex_; }

    // Whether the Transform tab's vertex fields/tools read and write
    // XForm::p instead of XForm::c - programmatic sync counterpart to
    // editingPostTransformChanged() below, for EditorWindow to call when
    // its own toolbar "Post" QAction toggles (does not re-emit the
    // signal - same "programmatic vs. user-driven" contract as
    // setSelectedXform() vs. selectedXformChanged() on TriangleCanvas).
    void setEditingPostTransform(bool post);
    bool editingPostTransform() const { return editingPost_; }

    // Re-reads the Transform tab's vertex fields from the selected xform's
    // active matrix (XForm::c or XForm::p) without touching any other tab
    // - for EditorWindow to call after a TriangleCanvas drag, which edits
    // c/p directly and bypasses this panel entirely (unlike every other
    // edit here, which always originates *from* this panel).
    void refreshTransformTabFromCanvas() { refreshTransformTab(); }

signals:
    // Fired continuously while a value is being edited (every keystroke/
    // spin tick/table-cell commit) - the owning window should treat this
    // exactly like TriangleCanvas::xformEdited (request a fresh preview
    // render, coalescing bursts).
    void propertyEdited();
    // Bracket one undo-worthy edit gesture - editingStarted() fires once
    // at the start (first change since the last commit), editingFinished()
    // once at the end (focus-out/Enter for text fields, or immediately
    // after a table-cell edit, which is already atomic). Matches
    // TriangleCanvas::editingStarted/editingFinished's role for drags.
    void editingStarted();
    void editingFinished();
    // Fired when the user toggles the Transform tab's own "Edit post
    // transform" checkbox - EditorWindow mirrors this onto its toolbar
    // "Post" QAction (and TriangleCanvas::setEditingPostTransform) so
    // canvas dragging follows suit.
    void editingPostTransformChanged(bool post);

private slots:
    void onWeightChanged(double value);
    void onWeightEditingFinished();
    void onNameEdited(const QString& text);
    void onNameEditingFinished();
    void onVariationCellChanged(QTableWidgetItem* item);
    void onVariationSearchChanged(const QString& text);
    void onHideUnusedToggled(bool hide);
    void onShowDescriptionsToggled(bool show);
    void onClearVariationsClicked();
    void onVariableCellChanged(QTableWidgetItem* item);
    void onColorChanged(double value);
    void onColorEditingFinished(double value);
    void onColorSpeedChanged(double value);
    void onColorSpeedEditingFinished(double value);
    void onOpacityChanged(double value);
    void onOpacityEditingFinished(double value);
    void onDirectColorChanged(double value);
    void onDirectColorEditingFinished(double value);
    void onSoloToggled(bool solo);
    void onEditPostToggled(bool post);
    void onVertexFieldChanged();
    void onVertexFieldEditingFinished();
    void onRotateByClicked();
    void onScaleByClicked();
    void onRotate90CWClicked();
    void onRotate90CCWClicked();
    void onFlipHorizontalClicked();
    void onFlipVerticalClicked();

private:
    QWidget* buildVariationsTab();
    QWidget* buildColorsTab();
    QWidget* buildVariablesTab();
    QWidget* buildTransformTab();

    apo::XForm* currentXForm() const;
    void refreshHeaderFields();
    void refreshVariationsValues();
    void applyVariationsRowFilter();
    void refreshColorsTab();
    void refreshColorSwatch();
    void refreshVariablesValues();
    void refreshTransformTab();
    // XForm::c if !editingPost_, XForm::p if editingPost_ - what the
    // Transform tab's vertex fields and rotate/scale/flip tools operate on.
    std::array<std::array<double, 2>, 3>& activeMatrix(apo::XForm& xf) const;
    void applyVertexEdit();
    // Shared implementation for the rotate/scale/rotate-90/flip tools:
    // reads the active matrix as a Triangle, applies `op` (given the
    // triangle's own centroid as pivot) to all three vertices, writes the
    // result back - one undo-worthy gesture per call (button clicks are
    // already atomic, unlike the vertex spin boxes' live-drag typing).
    void transformActiveTriangle(const std::function<apo::Point2(apo::Point2 pivot, apo::Point2 p)>& op);
    void beginEditIfNeeded();
    void commitEditIfNeeded();

    std::shared_ptr<apo::Flame> flame_;
    int selectedIndex_ = -1;
    bool hasPendingGesture_ = false;

    QLineEdit* nameEdit_ = nullptr;
    QDoubleSpinBox* weightSpin_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QTableWidget* variationsTable_ = nullptr;
    QLineEdit* variationSearchEdit_ = nullptr;
    QCheckBox* hideUnusedCheck_ = nullptr;
    QCheckBox* showDescriptionsCheck_ = nullptr;
    QPushButton* clearVariationsButton_ = nullptr;

    SliderSpin* colorSpin_ = nullptr;
    QLabel* colorSwatch_ = nullptr;
    SliderSpin* colorSpeedSpin_ = nullptr;
    SliderSpin* opacitySpin_ = nullptr;
    SliderSpin* directColorSpin_ = nullptr;
    QCheckBox* soloCheck_ = nullptr;

    QTableWidget* variablesTable_ = nullptr;

    bool editingPost_ = false;
    QCheckBox* editPostCheck_ = nullptr;
    QDoubleSpinBox* vertexOX_ = nullptr;
    QDoubleSpinBox* vertexOY_ = nullptr;
    QDoubleSpinBox* vertexXX_ = nullptr;
    QDoubleSpinBox* vertexXY_ = nullptr;
    QDoubleSpinBox* vertexYX_ = nullptr;
    QDoubleSpinBox* vertexYY_ = nullptr;
    QDoubleSpinBox* rotateBySpin_ = nullptr;
    QDoubleSpinBox* scaleBySpin_ = nullptr;
};

} // namespace apo::ui
