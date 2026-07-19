#pragma once

#include <memory>
#include <vector>

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
class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QListWidget;
class QTimer;
class QShowEvent;

namespace apo::ui {

class RenderWorker;

// A1's Save All Parameters (MainWindow.cpp) closed the "can't persist a
// curated library" gap; this closes the equivalent render-side gap - the
// original's Flame > "Render All Flames" (Main.dfm mnuRenderAll), absent
// from this port until now (RenderDialog only ever renders the one flame
// it was constructed with). Rather than a checkbox bolted onto
// RenderDialog (whose whole model - one flame, one output *file* - doesn't
// fit "many flames, one output *folder*"), this is its own small dialog:
// shared width/height/density/oversample/filter/adaptive-filter/
// transparent-background settings (applied identically to every flame via
// Flame::adjustScale(), the same call RenderDialog::startRender() makes
// for its own single flame) get applied to every flame in the library in
// turn, sequentially (one RenderWorker::renderFull call at a time, matching
// RenderDialog's own single-render-in-flight model and memory profile -
// see FOLLOWUP_PLAN.txt's A3 entry), each written to its own PNG inside a
// chosen output folder, named after the flame (sanitized for filesystem
// legality, de-duplicated up front for same/empty-named library entries).
class RenderAllDialog final : public QDialog {
    Q_OBJECT

public:
    explicit RenderAllDialog(std::vector<std::shared_ptr<apo::Flame>> flames, QWidget* parent = nullptr);
    ~RenderAllDialog() override;

    // Same verification-only hook as RenderDialog::setAutoScreenshot - see
    // its own doc comment.
    void setAutoScreenshot(const QString& path, bool exitAfter);

protected:
    void showEvent(QShowEvent* event) override;

signals:
    // Identical contract to RenderDialog::fullRenderRequested - see its own
    // doc comment (progress is owned by this dialog, not the receiver).
    void fullRenderRequested(std::shared_ptr<const apo::Flame> flame, quint64 seed, int threadCount,
                              apo::RenderProgress* progress, QString outputPath);

private slots:
    void onFullRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted, bool cancelled,
                               bool saved);
    void onProgressTick();
    void browseOutputFolder();
    void startRenderAll();
    void cancelRenderAll();

private:
    void setControlsEnabled(bool enabled);
    void updateStartButtonEnabled();
    // Kicks off flames_[currentIndex_] once onFullRenderFinished (or
    // startRenderAll() itself, for the very first one) is ready for the
    // next one - or, once currentIndex_ runs past the end, reports the
    // batch as finished and re-enables the controls.
    void renderNext();

    std::vector<std::shared_ptr<apo::Flame>> flames_;
    // Precomputed once at construction (see sanitizeAndDedupeNames() in the
    // .cpp) - every flame's own eventual "<name>.png" is decided up front,
    // not recomputed mid-batch, so the results list (resultsList_) can show
    // the real output filename before a single render has actually run.
    std::vector<QString> outputFileNames_;

    QSpinBox* widthSpin_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
    QDoubleSpinBox* densitySpin_ = nullptr;
    QSpinBox* oversampleSpin_ = nullptr;
    QDoubleSpinBox* filterRadiusSpin_ = nullptr;
    QCheckBox* adaptiveFilterCheck_ = nullptr;
    QCheckBox* transparentBackgroundCheck_ = nullptr;
    QLineEdit* outputFolderEdit_ = nullptr;
    QPushButton* browseButton_ = nullptr;
    QCheckBox* saveParametersCheck_ = nullptr;
    QListWidget* resultsList_ = nullptr;

    QLabel* overallLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* startButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;

    QThread* workerThread_ = nullptr;
    RenderWorker* worker_ = nullptr;

    bool batchInFlight_ = false;
    // -1 = idle (no batch started yet, or a prior one already finished);
    // otherwise the flames_/outputFileNames_ index currently in flight or
    // about to be.
    int currentIndex_ = -1;
    int succeededCount_ = 0;
    int failedCount_ = 0;
    bool cancelledMidBatch_ = false;

    std::unique_ptr<apo::RenderProgress> progress_;
    QTimer* progressTimer_ = nullptr;
    QElapsedTimer elapsedTimer_;
    // Same reason RenderDialog carries pendingRenderFlame_: onFullRenderFinished
    // needs the exact clone that was actually rendered (for the optional
    // "save parameters" .flame write), not flames_[currentIndex_]'s own,
    // possibly-different original field values.
    std::shared_ptr<const apo::Flame> pendingRenderFlame_;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
    bool autoScreenshotFired_ = false;
};

} // namespace apo::ui
