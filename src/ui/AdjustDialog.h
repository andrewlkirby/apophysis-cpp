#pragma once

#include <array>
#include <memory>
#include <vector>

#include <QDialog>
#include <QImage>
#include <QString>

#include "core/ColorMap.h"
#include "core/Flame.h"

class QThread;
class QResizeEvent;
class QCloseEvent;
class QLabel;
class QAction;

namespace apo::ui {

class RenderWorker;
class SliderSpin;
class AdjustPanel;

// Port of Adjust.pas (2,733 lines in the original) - deliberately scoped
// down per the "good performance, not a faithful copy" project directive.
// Kept: Camera (zoom/center/angle/3D camera), Coloring (gamma/brightness/
// vibrancy/background/gamma threshold), Gradient (rotate/hue/saturation/
// brightness/contrast/blur/frequency/invert/reverse/randomize), and Size
// tabs, plus a live-updating preview and its own undo/redo stack. Dropped:
// the Curves tab (already scoped to Phase 7's utility-dialog batch per the
// migration plan, not this dialog), direct mouse-drag gradient
// stretch/rotate on the gradient strip (redundant with the Rotate mode
// slider), registry-persisted image-size presets, and clipboard gradient
// copy/paste (all low-value relative to their port cost here).
//
// Unlike the original's OK/Cancel-less "live apply to MainCp" model with a
// *separate* global undo (its own Undo/Redo buttons just called
// MainForm.Undo/Redo), this dialog owns a self-contained undo/redo stack of
// whole-flame-parameter snapshots, mutating the same shared Flame the
// owning EditorWindow displays - simpler to reason about than threading a
// second dialog's edits through another window's undo stack.
//
// The actual Camera/Coloring/Gradient/Size controls live in AdjustPanel
// (see its own header comment) - this dialog just wraps one next to its own
// live preview label, toolbar Undo/Redo, and window geometry persistence,
// reacting to AdjustPanel's propertyEdited()/editingStarted()/
// editingFinished() signals the same way EditorWindow does for its own
// embedded AdjustPanel (see EditorWindow.h) - one control implementation,
// two hosts. Kept as a standalone top-level window (rather than deleted
// once EditorWindow could embed the panel directly) for the `--adjust` CLI
// entry point and this dialog's own test coverage
// (adjust_dialog_interaction_test.cpp) - both predate the embedded panel
// and don't need a Flame already open in an Editor window to exercise.
class AdjustDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AdjustDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent = nullptr);
    ~AdjustDialog() override;

    void setAutoScreenshot(const QString& path, bool exitAfter);

protected:
    void resizeEvent(QResizeEvent* event) override;
    // Persists the dialog's final size/position (see WindowGeometry.h).
    void closeEvent(QCloseEvent* event) override;

signals:
    // Emitted after every committed edit so an owning window (EditorWindow)
    // can refresh its own view of the same shared Flame.
    void flameChanged();
    void renderRequested(std::shared_ptr<const apo::Flame> flame, quint64 seed);

private slots:
    void onRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted);
    void onUndo();
    void onRedo();

private:
    struct FlameSnapshot {
        double zoom = 0, centerX = 0, centerY = 0, angle = 0;
        double pitch = 0, yaw = 0, persp = 0, dof = 0, zpos = 0;
        double gamma = 0, brightness = 0, vibrancy = 0, gammaThreshold = 0;
        std::array<int, 4> background{};
        apo::ColorMap cmap;
        int width = 0, height = 0;
        double pixelsPerUnit = 0;

        bool operator==(const FlameSnapshot& other) const;
    };
    struct UndoEntry {
        FlameSnapshot before, after;
    };

    FlameSnapshot snapshot() const;
    void applySnapshot(const FlameSnapshot& s);

    // Bracket one AdjustPanel gesture into this dialog's own undo stack -
    // connected straight to AdjustPanel::editingStarted()/editingFinished().
    // beginEdit() runs synchronously inside AdjustPanel::beginEditIfNeeded(),
    // before that call's own mutation, so the snapshot captured here is
    // always the true pre-edit state (see AdjustPanel.h's contract).
    void beginEdit();
    // Pushes an undo entry if anything actually changed, clears the redo
    // stack, and notifies the owning window - the panel-hosting equivalent
    // of the old self-contained commitEdit().
    void commitEdit();
    void updateUndoRedoActions();

    void requestPreviewRender();

    std::shared_ptr<apo::Flame> flame_;
    AdjustPanel* panel_ = nullptr;

    QLabel* previewLabel_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;

    std::vector<UndoEntry> undoStack_;
    std::vector<UndoEntry> redoStack_;
    bool hasPendingSnapshot_ = false;
    FlameSnapshot pendingBefore_;

    QThread* workerThread_ = nullptr;
    RenderWorker* worker_ = nullptr;
    bool renderInFlight_ = false;
    bool renderDirty_ = false;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
};

} // namespace apo::ui
