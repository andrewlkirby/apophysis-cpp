#pragma once

#include <memory>

#include <QDialog>
#include <QString>

#include "core/Flame.h"

class QTableWidget;
class QTableWidgetItem;
class QShowEvent;

namespace apo::ui {

// Port of Editor.pas's Xaos tab (the vleChaos TValueListEditor grid plus
// its ChaosPopup menu) - editing the per-xform-pair transition weight
// matrix (XForm::modWeights, "xaos"). A deliberate redesign, not a line-
// for-line port, per the "good performance, not a faithful copy" project
// direction:
//   - The original shows only the *currently selected* xform's row of
//     weights at a time, toggled between a "to" view (that xform's
//     outgoing weights) and a "from" view (every other xform's weight
//     into it) via a menu radio pair - a workaround for TValueListEditor
//     only ever displaying one flat list. A modern QTableWidget has no
//     such limitation, so this port shows the *entire* NxN weight matrix
//     at once (rows = source xform, columns = target xform, cell [i][j] =
//     xform i's weight toward xform j) - strictly more information,
//     visible without a mode toggle, and edits any pair directly. This
//     also sidesteps needing to track the Editor's canvas selection at
//     all - the dialog is fully self-contained.
//   - Dropped DeleteTriangle's optional RebuildXaosLinks smart-relink step
//     - see Flame::removeXForm's own doc comment (Xaos deletion behavior
//     lives in the core method now, not here).
//   - Clear All / Set All apply to the *whole* matrix (every cell to 0 or
//     1 respectively) rather than just the selected xform's row/column,
//     the natural generalization once there's no "selected xform" concept
//     driving the view.
//
// Like AdjustDialog/MutateDialog/CurvesDialog (and unlike PostProcessDialog),
// this mutates the shared flame_ in place and relies on the owning
// EditorWindow's own live preview to reflect edits - it has no preview of
// its own, matching the original (the Xaos tab never had a dedicated
// preview either; it just triggered the already-visible Editor's
// UpdateFlame).
//
// Unlike every other Editor dialog, EditorWindow keeps this one alive
// (not fire-and-forget) so it can be refreshed via refreshFromFlame() when
// an Add/Duplicate/Delete xform (or an undo/redo of one) changes the
// active xform count while this dialog is still open - see
// EditorWindow::openXaosDialog.
class XaosDialog final : public QDialog {
    Q_OBJECT

public:
    explicit XaosDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent = nullptr);

    void setAutoScreenshot(const QString& path, bool exitAfter);

    // Rebuilds the table from flame_'s current xform count/weights - call
    // after a structural edit (Add/Duplicate/Delete/undo/redo) applied
    // elsewhere while this dialog is open, so it doesn't keep showing
    // stale rows/columns for xforms that no longer exist at those indices.
    void refreshFromFlame();

protected:
    void showEvent(QShowEvent* event) override;

signals:
    // Emitted after every committed edit so the owning EditorWindow can
    // refresh its own live preview of the same shared Flame.
    void flameChanged();

private slots:
    void onCellChanged(QTableWidgetItem* item);

private:
    void rebuildTable();
    void onClearAll();
    void onSetAll();

    std::shared_ptr<apo::Flame> flame_;
    QTableWidget* table_ = nullptr;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
    bool autoScreenshotFired_ = false;
};

} // namespace apo::ui
