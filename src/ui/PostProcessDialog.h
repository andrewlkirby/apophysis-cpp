#pragma once

#include <memory>

#include <QDialog>
#include <QElapsedTimer>
#include <QImage>
#include <QString>

#include "core/Flame.h"
#include "core/render/Renderer.h"

class QThread;
class QLabel;
class QPushButton;
class QProgressBar;
class QTimer;
class QResizeEvent;

namespace apo::ui {

class RenderWorker;
class SliderSpin;

// Port of Forms/formPostProcess.pas (523 lines) - tone-mapping-only tuning
// (Filter Radius/Gamma/Brightness/Contrast/Vibrancy/Background) on a
// dedicated preview, plus a Save-to-PNG action. Deliberately scoped down
// per the "good performance, not a faithful copy" project direction:
//   - Dropped the DragPanel click-and-drag-horizontally-on-a-label value
//     scrubber (DragPanelMouseDown/Move/Up) - SliderSpin (already used
//     identically across Adjust/Mutate/Curves) covers the same drag-to-
//     adjust need with a visible, discoverable control, so a second bespoke
//     input widget isn't worth porting.
//   - Dropped the explicit Apply button and its manual TryStrToFloat/clamp
//     logic on raw text fields - SliderSpin's QDoubleSpinBox already
//     clamps to a fixed range and applies live, same reasoning as every
//     other dialog in this port.
//   - Dropped the "Fit to Window" checkbox - chkFitToWindowClick's body is
//     entirely commented out in the original itself (a dead, never-
//     functional control), not something to port.
//   - Dropped Width/Height/Sample Density/Oversample - those are
//     RenderDialog's job; this dialog reuses whatever size/density the
//     flame is already configured with for its Save action, rather than
//     duplicating RenderDialog's own size UI a second time.
//   - Kept (and the whole reason this is still its own dialog rather than
//     folded into AdjustDialog): the original's most important structural
//     property, that this dialog **never writes back to the flame it was
//     opened on** - formPostProcess.SetControlPoint does `FCP := cp.Clone`,
//     and every edit only ever touches that private clone, never the
//     shared MainCp. This port preserves that (see flame_ below) - a
//     deliberate difference from Adjust/Mutate/Curves, which all mutate
//     the shared Flame in place: PostProcess's job is tuning one *output
//     image*, not the flame's saved parameters, so there is no
//     flameChanged signal here.
class PostProcessDialog final : public QDialog {
    Q_OBJECT

public:
    // Takes the flame by const shared_ptr - this dialog only ever reads it
    // once, via Flame::clone() (a const method), to seed its own private
    // clone (see flame_ below), so it can be handed either a live, mutable
    // Flame (EditorWindow's shared flame_) or an already-const one (e.g.
    // RenderDialog's just-rendered lastRenderedFlame_) without the caller
    // needing a non-const copy just to satisfy this signature.
    explicit PostProcessDialog(std::shared_ptr<const apo::Flame> flame, QWidget* parent = nullptr);
    ~PostProcessDialog() override;

    void setAutoScreenshot(const QString& path, bool exitAfter);

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void renderRequested(std::shared_ptr<const apo::Flame> flame, quint64 seed);
    // `progress` is a raw pointer owned by this dialog (progress_) - see
    // RenderWorker::renderFull's doc comment on the ownership contract.
    void fullRenderRequested(std::shared_ptr<const apo::Flame> flame, quint64 seed, int threadCount,
                              apo::RenderProgress* progress, QString outputPath);

private slots:
    void onRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted);
    void onFullRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted, bool cancelled,
                               bool saved);
    void onProgressTick();

private:
    void requestPreviewRender();
    void pickBackgroundColor();
    void refreshBackgroundSwatch();
    void onReset();
    void onSaveClicked();
    void setControlsEnabled(bool enabled);

    // Never the caller's shared Flame - see the class comment above.
    std::unique_ptr<apo::Flame> flame_;

    // Values captured at construction time, for Reset.
    double initialFilterRadius_ = 0;
    double initialGamma_ = 0, initialBrightness_ = 0, initialContrast_ = 0, initialVibrancy_ = 0;
    std::array<int, 4> initialBackground_{};

    SliderSpin* filterRadiusCtrl_ = nullptr;
    SliderSpin* gammaCtrl_ = nullptr;
    SliderSpin* brightnessCtrl_ = nullptr;
    SliderSpin* contrastCtrl_ = nullptr;
    SliderSpin* vibrancyCtrl_ = nullptr;
    QPushButton* backgroundButton_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QLabel* previewLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    QThread* workerThread_ = nullptr;
    RenderWorker* worker_ = nullptr;
    bool renderInFlight_ = false;
    bool renderDirty_ = false;

    bool fullRenderInFlight_ = false;
    std::unique_ptr<apo::RenderProgress> progress_;
    QTimer* progressTimer_ = nullptr;
    QElapsedTimer elapsedTimer_;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
};

} // namespace apo::ui
