#pragma once

#include <memory>

#include <QDialog>
#include <QElapsedTimer>
#include <QImage>
#include <QString>

#include "core/Flame.h"
#include "core/render/Renderer.h"

class QThread;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QTimer;
class QShowEvent;

namespace apo::ui {

class RenderWorker;

// Port of FormRender.pas (1,318 lines) - deliberately scoped down per the
// "good performance, not a faithful copy" project directive. Kept: image
// size (maintain aspect) with width/height/density preset combos (matches
// FormRender.dfm's cbWidth/cbHeight/txtDensity exactly - see
// buildSizeRow()'s own comment on why they're a *supplement* to the
// existing spin boxes here rather than a literal port of the original's
// editable-TComboBox-instead-of-a-spinner widget choice), sample density,
// oversample, filter radius, an Adaptive Filter checkbox (Flame::enableDE
// - see Renderer.h's class comment: a from-scratch working implementation,
// not a port, since the original's own adaptive-DE code is unreachable
// dead code), a Transparent Background checkbox (Flame::transparency -
// the original's own equivalent, Options>General "PNG transparency", is a
// global default rather than a per-render control; nothing else in this
// port ever exposed a way to set this at all until now), output filename
// (browse + save), a "Save parameters"
// checkbox (writes a .flame next to the PNG, with exactly the settings
// that were actually rendered - plan's P4.3), a live memory-usage estimate
// (Renderer::estimatePeakMemoryBytes(), recomputed on every size/
// oversample/filter-radius edit - plan's P4.1), Render/Pause/Cancel, a live
// progress bar backed by apo::RenderProgress's cooperative-cancellation and
// -pause tokens (Renderer.h), and a status line (points done, elapsed
// time). Pause blocks the render in place (RenderProgress::pauseRequested -
// see its own doc comment) rather than tearing anything down, so Resume
// picks up exactly where it left off with no lost work or restart, unlike
// Cancel.
// Windows shutdown-when-done, post-process integration (a separate Phase 7
// dialog), the binary/incomplete-render flam3 save-format toggles, the
// donate/save-log buttons, named/persisted presets (cmbPreset - a whole
// separate save/delete-preset feature, not just the three quick-value
// combos this port keeps), and - beyond simply toggling it on/off - any UI
// for tuning the adaptive filter's own estimator/estimatorMin/
// estimatorCurve shape parameters (still settable via the .flame file
// itself; a dedicated tuning UI wasn't part of the original's Render
// dialog either) - all either out of this port's scope or low-value
// relative to their port cost.
//
// Unlike the original's fields-live-directly-on-cp model, this dialog
// renders a *clone* of the given flame with the dialog's own size/
// density/oversample/filter-radius settings applied - so opening Render
// and raising the density for a final export doesn't silently change what
// the owning Editor window's live preview uses.
class RenderDialog final : public QDialog {
    Q_OBJECT

public:
    explicit RenderDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent = nullptr);
    ~RenderDialog() override;

    void setAutoScreenshot(const QString& path, bool exitAfter);

protected:
    void showEvent(QShowEvent* event) override;

signals:
    // `progress` is a raw pointer owned by this dialog (progress_) - see
    // RenderWorker::renderFull's doc comment on the ownership contract.
    // `threadCount` starts from AppSettings (OptionsDialog's Performance
    // group) at the moment Render is clicked, then gets passed through
    // MemoryBudget.h's resolveMemorySafeThreadCount() (FOLLOWUP_PLAN.txt
    // B6) - possibly reduced below the AppSettings value to keep this
    // render's estimated peak memory within budget; not live-tracked
    // afterward either way.
    void fullRenderRequested(std::shared_ptr<const apo::Flame> flame, quint64 seed, int threadCount,
                              apo::RenderProgress* progress, QString outputPath);

private slots:
    void onFullRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted, bool cancelled,
                               bool saved);
    void onProgressTick();
    void browseOutputPath();
    void startRender();
    void cancelRender();
    void togglePauseRender();

private:
    void setControlsEnabled(bool enabled);
    void updateRenderButtonEnabled();
    // Recomputes memoryEstimateLabel_ from the dialog's *current* field
    // values via Renderer::estimatePeakMemoryBytes() - connected to every
    // control that actually affects bucket sizing (width/height/
    // oversample/filter radius); sample density deliberately excluded,
    // since it changes how many points get iterated, not how large the
    // bucket array is.
    void updateMemoryEstimate();

    std::shared_ptr<apo::Flame> flame_;

    QSpinBox* widthSpin_ = nullptr;
    QComboBox* widthPresetCombo_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
    QComboBox* heightPresetCombo_ = nullptr;
    QCheckBox* maintainAspectCheck_ = nullptr;
    double aspectRatio_ = 1.0;
    QDoubleSpinBox* densitySpin_ = nullptr;
    QComboBox* densityPresetCombo_ = nullptr;
    QSpinBox* oversampleSpin_ = nullptr;
    QDoubleSpinBox* filterRadiusSpin_ = nullptr;
    QCheckBox* adaptiveFilterCheck_ = nullptr;
    // Flame::transparency - renders with an alpha channel instead of
    // compositing onto flame_->background, so the output PNG's empty areas
    // are transparent rather than opaque. Matches the original's
    // Options>General "PNG transparency" toggle in effect (also PNG-only,
    // since this dialog only ever writes PNGs - see outputPathEdit_'s own
    // filter), but exposed here per-render instead of as a global default,
    // since a global-only toggle would be a worse fit for this dialog's own
    // "render a clone with this dialog's own settings, not flame_'s live
    // ones" model (see this class's own doc comment) than a single checkbox
    // is.
    QCheckBox* transparentBackgroundCheck_ = nullptr;
    QLabel* memoryEstimateLabel_ = nullptr;
    QLineEdit* outputPathEdit_ = nullptr;
    QPushButton* browseButton_ = nullptr;
    // Plan's P4.3 - writes a .flame alongside the rendered PNG, next to it
    // (same basename, ".flame" extension), capturing exactly the settings
    // that were actually rendered (pendingRenderFlame_ below), not flame_'s
    // own possibly-different original values.
    QCheckBox* saveParametersCheck_ = nullptr;
    QPushButton* renderButton_ = nullptr;
    QPushButton* pauseButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    QThread* workerThread_ = nullptr;
    RenderWorker* worker_ = nullptr;

    bool fullRenderInFlight_ = false;
    std::unique_ptr<apo::RenderProgress> progress_;
    QTimer* progressTimer_ = nullptr;
    QElapsedTimer elapsedTimer_;
    // The exact clone startRender() launched RenderWorker::renderFull with
    // - onFullRenderFinished needs it (when saveParametersCheck_ is
    // checked) to write out the settings that were actually rendered, not
    // just re-read flame_'s own (possibly different) original values.
    std::shared_ptr<const apo::Flame> pendingRenderFlame_;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
    bool autoScreenshotFired_ = false;
};

} // namespace apo::ui
