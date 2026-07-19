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
class QEvent;
class QLabel;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QAction;

namespace apo::ui {

class RenderWorker;
class SliderSpin;

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
class AdjustDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AdjustDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent = nullptr);
    ~AdjustDialog() override;

    void setAutoScreenshot(const QString& path, bool exitAfter);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

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

    QWidget* buildCameraTab();
    QWidget* buildColoringTab();
    QWidget* buildGradientTab();
    QWidget* buildSizeTab();

    FlameSnapshot snapshot() const;
    void applySnapshot(const FlameSnapshot& s);
    void refreshControlsFromFlame();
    void refreshGradientStrip();
    void refreshBackgroundSwatch();

    // Call before mutating flame_ from any control's live-change handler -
    // lazily captures the pre-edit snapshot on the first change of a
    // gesture (slider drag, spin box typing session, button click).
    void beginEditIfNeeded();
    // Call once a gesture completes (slider released, spin box editing
    // finished, or immediately after a single-shot button action) - pushes
    // an undo entry if anything actually changed, clears the redo stack,
    // requests a fresh preview, and notifies the owning window.
    void commitEdit();
    void updateUndoRedoActions();

    void requestPreviewRender();
    void pickBackgroundColor();
    void setGradientMode(int modeIndex);
    void applyGradientAmount(double amount);
    void openGradientBrowser();

    std::shared_ptr<apo::Flame> flame_;
    apo::ColorMap initialCmap_;   // for the Gradient tab's Reset button
    apo::ColorMap gradientBaseline_; // snapshot the current mode's slider amount is applied on top of

    // Camera tab
    SliderSpin* zoomCtrl_ = nullptr;
    SliderSpin* centerXCtrl_ = nullptr;
    SliderSpin* centerYCtrl_ = nullptr;
    SliderSpin* angleCtrl_ = nullptr;
    SliderSpin* pitchCtrl_ = nullptr;
    SliderSpin* yawCtrl_ = nullptr;
    SliderSpin* perspCtrl_ = nullptr;
    SliderSpin* dofCtrl_ = nullptr;
    SliderSpin* zposCtrl_ = nullptr;

    // Coloring tab
    SliderSpin* gammaCtrl_ = nullptr;
    SliderSpin* brightnessCtrl_ = nullptr;
    SliderSpin* vibrancyCtrl_ = nullptr;
    SliderSpin* gammaThresholdCtrl_ = nullptr;
    QPushButton* backgroundButton_ = nullptr;

    // Gradient tab
    QLabel* gradientStrip_ = nullptr;
    QComboBox* gradientModeCombo_ = nullptr;
    SliderSpin* gradientAmountCtrl_ = nullptr;

    // Size tab
    QSpinBox* widthSpin_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
    QCheckBox* maintainAspectCheck_ = nullptr;
    double sizeAspectRatio_ = 1.0;

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
