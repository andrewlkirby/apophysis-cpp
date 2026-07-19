#pragma once

#include <array>
#include <memory>
#include <vector>

#include <QImage>
#include <QMainWindow>
#include <QString>

#include "core/Flame.h"

class QListWidget;
class QThread;
class QAction;
class QResizeEvent;

namespace apo::ui {

class RenderWorker;
class TriangleCanvas;
class AdjustDialog;
class RenderDialog;
class MutateDialog;
class FullscreenView;
class CurvesDialog;
class PostProcessDialog;
class XaosDialog;
class ForceSymmetryDialog;
class TransformPanel;

// The Editor window: TriangleCanvas (see its own file for the interaction
// model) plus a live preview render behind it, an xform list, a
// Move/Rotate/Scale mode toolbar, pivot-picking, and undo/redo. Opened
// from MainWindow (double-click a flame in the library list).
//
// requestRender() renders a *preview* clone of flame_ - scaled down to the
// canvas's own on-screen pixel size (TriangleCanvas::setBackgroundImage's
// background_ is stretched to fill the widget regardless of its actual
// resolution, so this costs nothing visually) and capped to
// AppSettings::previewSampleDensity() - not flame_ itself at its own
// declared width/height/sampleDensity. This was a real, not just
// theoretical, gap until fixed: every triangle drag re-renders on every
// mouse-move (see xformEdited's doc comment), so rendering flame_ as-is
// meant a flame authored at e.g. 1920x1080/sampleDensity=50 re-rendered at
// full production quality on every single pixel of mouse movement -
// unusably slow for anything but a toy-sized test flame. Matches the
// original's own PreviewDensity mechanism for exactly this workflow (see
// OPTIMIZATION_STATUS.md fix #11's mention of it), reusing the same shared
// preview-quality knob AdjustDialog/MutateDialog already use rather than a
// separate Editor-only setting - matches AppSettings.h's own "a user is
// unlikely to want them independently configured" reasoning.
class EditorWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit EditorWindow(std::shared_ptr<apo::Flame> flame, QWidget* parent = nullptr);
    ~EditorWindow() override;

    // Testing/verification hook, not a normal user-facing feature - same
    // idea as MainWindow::setAutoScreenshot: once the current render
    // completes, grab the window and save it, then (if `exitAfter`) quit
    // the application. This is what let the triangle canvas actually be
    // verified by launching the real GUI and looking at its output,
    // rather than just by reading the code.
    void setAutoScreenshot(const QString& path, bool exitAfter);

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void renderRequested(std::shared_ptr<const apo::Flame> flame, quint64 seed);

private slots:
    void onXformEdited(int index);
    void onEditingStarted(int index);
    void onEditingFinished(int index);
    void onRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted);
    void onXformListSelectionChanged(int row);
    void onCanvasSelectionChanged(int index);
    void onUndo();
    void onRedo();
    void openAdjustDialog();
    void openRenderDialog();
    void openMutateDialog();
    void onSaveFlameAsTriggered();
    void openFullscreenView();
    void openCurvesDialog();
    void openPostProcessDialog();
    void onAddXform();
    void onDuplicateXform();
    void onDeleteXform();
    // In-app transform clipboard (plan item 1.8) - copies/pastes every
    // field of the selected xform (affine coefs, variations, colors,
    // name...) via XForm::assign(), same as Duplicate but writing into the
    // *currently selected* xform instead of a freshly appended one, and
    // sourced from whatever was last copied instead of the xform being
    // duplicated. Deliberately not the OS clipboard/a text interchange
    // format - see EditorWindow.cpp's onCopyXform() for why.
    void onCopyXform();
    void onPasteXform();
    void openXaosDialog();
    void openForceSymmetryDialog();
    void onForceSymmetryRequested(int sym);
    // TransformPanel's property-edit gesture brackets (weight/name/
    // variation edits) - same undo-snapshot role as onEditingStarted/
    // onEditingFinished play for triangle drags, but capturing the whole
    // flame (Structural kind) since a property edit isn't expressible as
    // just one xform's c/p matrices.
    void onXformPropertyEditingStarted();
    void onXformPropertyEditingFinished();
    // The toolbar's checkable "Final Transform" action - enables/disables
    // Flame::finalXformEnabled/useFinalXform together (this port collapses
    // the original's two separate flags into one user-facing toggle; see
    // Flame.h's fields for why they exist separately at the model layer)
    // and selects/deselects the final transform on the canvas to match.
    void onFinalXformToggled(bool enabled);
    // A6 - Main.pas's Flame-menu weight/color utility commands (see
    // core/edit/FlameOps.h's own doc comment on what each does and why
    // "Compute Weights" is deliberately not among them). Each is a
    // Structural undo entry (whole-flame before/after, selection
    // unchanged), same shape as onPasteXform's in-place edit.
    void onRandomizeWeights();
    void onEqualizeWeights();
    void onCalculateColorValues();
    void onRandomizeColorValues();

private:
    void requestRender();
    void refreshXformList();
    // xformIndexForListRow()/listRowForXformIndex() convert between
    // xformList_'s row numbers (0..numXForms()-1 for regular xforms, then
    // one extra trailing "Final" row when Flame::finalXformEnabled) and
    // the xform-index space TriangleCanvas/TransformPanel/undo entries use
    // (0..numXForms()-1, or apo::kFinalXformIndex) - kept as two small
    // helpers rather than inlining the mapping at each of the several call
    // sites that need it in both directions.
    int xformIndexForListRow(int row) const;
    int listRowForXformIndex(int index) const;
    // *flame_->xform[index], or *flame_->finalXform if index is
    // apo::kFinalXformIndex - the same sentinel-aware resolution
    // TriangleCanvas::triangleFor()/writeTriangle() already do, needed
    // here too since onEditingStarted/onEditingFinished/onUndo/onRedo all
    // read/write an xform's c/p matrices directly by whatever index a
    // TriangleCanvas drag reports.
    apo::XForm& xformAt(int index);
    void updateUndoRedoActions();
    // Called after any structural edit (Add/Duplicate/Delete or an
    // undo/redo of one) so an already-open XaosDialog doesn't keep
    // showing stale rows/columns for xforms that no longer exist at those
    // indices - see XaosDialog.h's class comment on why this is the one
    // Editor dialog EditorWindow keeps a live pointer to instead of
    // firing-and-forgetting.
    void refreshXaosDialogIfOpen();
    // Pushes a Structural undo entry capturing the whole flame before/after
    // a structural edit already applied to flame_ - shared by
    // onAddXform/onDuplicateXform/onDeleteXform. `selectAfterUndo` is
    // whatever was selected before the edit; `selectAfterRedo` is the
    // xform the edit itself should leave selected (the new/duplicated
    // xform, or the compacted neighbor after a delete).
    void pushStructuralUndo(const apo::Flame& before, int selectAfterUndo, int selectAfterRedo);

    // One undo-history entry. Two unrelated kinds share one stack so
    // Ctrl+Z/Ctrl+Shift+Z always undoes/redoes whatever was done most
    // recently, whether that was a triangle drag or an Add/Duplicate/
    // Delete xform:
    //   - Drag: the selected xform's affine (c) and post (p) coefficient
    //     matrices before and after one completed drag (see
    //     TriangleCanvas::editingStarted/editingFinished's doc comments for
    //     why "one drag" is the chosen granularity). Both matrices are
    //     always captured regardless of which one the drag actually
    //     touched - simpler and cheap (12 doubles) rather than tracking
    //     which of c/p was in play at snapshot time.
    //   - Structural: a full-flame clone before and after an Add/Duplicate/
    //     Delete, since those renumber every xform at or after the edited
    //     index (see Flame::removeXForm's column/row-compaction) - no
    //     single-xform diff can express that, so this reuses Flame::clone/
    //     copyFrom (the same proven deep-copy already relied on elsewhere,
    //     e.g. MutateDialog adopting a mutant) rather than writing a
    //     bespoke partial-snapshot format. Heavier per entry than a Drag
    //     entry, but Add/Duplicate/Delete are discrete, low-frequency
    //     clicks, not something dragged continuously - the cost is a
    //     non-issue.
    enum class UndoKind { Drag, Structural };
    struct UndoEntry {
        UndoKind kind = UndoKind::Drag;

        int xformIndex = -1;
        std::array<std::array<double, 2>, 3> beforeC{}, beforeP{}, afterC{}, afterP{};

        std::shared_ptr<apo::Flame> beforeFlame, afterFlame;
        int selectAfterUndo = -1, selectAfterRedo = -1;
    };

    std::shared_ptr<apo::Flame> flame_;

    TriangleCanvas* canvas_ = nullptr;
    QListWidget* xformList_ = nullptr;
    TransformPanel* transformPanel_ = nullptr;

    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* pickPivotAction_ = nullptr;
    QAction* clearPivotAction_ = nullptr;
    QAction* addXformAction_ = nullptr;
    QAction* duplicateXformAction_ = nullptr;
    QAction* deleteXformAction_ = nullptr;
    QAction* finalXformAction_ = nullptr;

    // Not fire-and-forget like the other Editor dialogs (raw pointer,
    // cleared via QObject::destroyed when the user closes it) - see
    // refreshXaosDialogIfOpen()'s doc comment.
    XaosDialog* xaosDialog_ = nullptr;

    std::vector<UndoEntry> undoStack_;
    std::vector<UndoEntry> redoStack_;
    bool hasPendingEntry_ = false;
    UndoEntry pendingEntry_;

    bool hasPendingPropertyEdit_ = false;
    std::unique_ptr<apo::Flame> pendingPropertyBefore_;

    QThread* workerThread_ = nullptr;
    RenderWorker* worker_ = nullptr;
    bool renderInFlight_ = false;
    bool renderDirty_ = false;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
};

} // namespace apo::ui
