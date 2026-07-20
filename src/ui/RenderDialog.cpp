#include "RenderDialog.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <random>
#include <thread>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "FileDialogSupport.h"
#include "MemoryBudget.h"
#include "RenderWorker.h"
#include "core/io/FlameIO.h"

namespace apo::ui {

namespace {
// How often the UI thread polls the worker thread's RenderProgress atomics
// - frequent enough to feel live, far below the cost of a QTimer tick.
constexpr int kProgressPollIntervalMs = 150;

// Human-readable byte count (matches the everyday "MB"/"GB" a user expects
// from a memory estimate, not IEC KiB/MiB pedantry).
QString formatBytes(std::uint64_t bytes) {
    constexpr double kKB = 1024.0, kMB = kKB * 1024.0, kGB = kMB * 1024.0;
    const double b = static_cast<double>(bytes);
    if (b >= kGB) return QString::number(b / kGB, 'f', 2) + " GB";
    if (b >= kMB) return QString::number(b / kMB, 'f', 1) + " MB";
    if (b >= kKB) return QString::number(b / kKB, 'f', 0) + " KB";
    return QString::number(bytes) + " B";
}

// Same basename as `pngPath`, ".flame" extension - matches plan's P4.3
// wording ("writes the .flame next to the image") exactly.
QString flameParametersPathFor(const QString& pngPath) {
    if (pngPath.endsWith(".png", Qt::CaseInsensitive)) return pngPath.left(pngPath.length() - 4) + ".flame";
    return pngPath + ".flame";
}

// Matches FormRender.dfm's cbWidth/cbHeight/txtDensity Items.Strings
// exactly (verified directly against source).
constexpr int kWidthPresets[] = {320, 640, 800, 1024, 1280, 1600, 1920, 2048, 2560, 3200};
constexpr int kHeightPresets[] = {200, 240, 480, 600, 768, 1024, 1200, 1920, 2048, 2400};
constexpr int kDensityPresets[] = {200, 500, 1000, 2000, 4000};

// A quick-pick companion to (not a replacement for) the existing spin box
// - the original's own cbWidth/cbHeight/txtDensity are plain editable
// TComboBoxes (type a value OR pick a preset, no separate spinner), but
// swapping this port's QSpinBox/QDoubleSpinBox out for an editable
// QComboBox would lose the increment/decrement arrows and validated-range
// input those already give for free, and would break every existing test
// that finds these controls by their QSpinBox/QDoubleSpinBox type. A
// narrow "Preset" combo next to each spin box delivers the same practical
// value (fast access to the same common values) without that churn.
// Leading "Preset..." placeholder so the combo never looks like it's
// showing a live/current value - it isn't, the spin box is.
QComboBox* makePresetCombo(QWidget* parent, const char* objectName, std::initializer_list<int> values) {
    auto* combo = new QComboBox(parent);
    combo->setObjectName(objectName);
    combo->addItem("Preset...");
    for (int v : values) combo->addItem(QString::number(v));
    return combo;
}
} // namespace

RenderDialog::RenderDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent)
    : QDialog(parent), flame_(std::move(flame)) {
    setWindowTitle(
        QString("Render - %1").arg(flame_->name.empty() ? "Untitled" : QString::fromStdString(flame_->name)));
    resize(420, 360);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* rootLayout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    widthSpin_ = new QSpinBox(this);
    widthSpin_->setObjectName("widthSpin");
    widthSpin_->setRange(1, 16384);
    widthSpin_->setValue(flame_->width);
    widthPresetCombo_ = makePresetCombo(this, "widthPresetCombo", {320, 640, 800, 1024, 1280, 1600, 1920, 2048, 2560, 3200});
    auto* widthRow = new QHBoxLayout();
    widthRow->addWidget(widthSpin_, 1);
    widthRow->addWidget(widthPresetCombo_);
    form->addRow("Width", widthRow);

    heightSpin_ = new QSpinBox(this);
    heightSpin_->setObjectName("heightSpin");
    heightSpin_->setRange(1, 16384);
    heightSpin_->setValue(flame_->height);
    heightPresetCombo_ = makePresetCombo(this, "heightPresetCombo", {200, 240, 480, 600, 768, 1024, 1200, 1920, 2048, 2400});
    auto* heightRow = new QHBoxLayout();
    heightRow->addWidget(heightSpin_, 1);
    heightRow->addWidget(heightPresetCombo_);
    form->addRow("Height", heightRow);

    aspectRatio_ = static_cast<double>(flame_->width) / std::max(1, flame_->height);

    maintainAspectCheck_ = new QCheckBox("Maintain aspect ratio", this);
    maintainAspectCheck_->setObjectName("maintainAspectCheck");
    maintainAspectCheck_->setChecked(true);
    form->addRow(maintainAspectCheck_);

    densitySpin_ = new QDoubleSpinBox(this);
    densitySpin_->setObjectName("densitySpin");
    densitySpin_->setRange(1, 1000000);
    densitySpin_->setDecimals(1);
    densitySpin_->setValue(flame_->sampleDensity);
    densityPresetCombo_ = makePresetCombo(this, "densityPresetCombo", {200, 500, 1000, 2000, 4000});
    auto* densityRow = new QHBoxLayout();
    densityRow->addWidget(densitySpin_, 1);
    densityRow->addWidget(densityPresetCombo_);
    form->addRow("Sample Density", densityRow);

    oversampleSpin_ = new QSpinBox(this);
    oversampleSpin_->setObjectName("oversampleSpin");
    oversampleSpin_->setRange(1, 8);
    oversampleSpin_->setValue(std::max(1, flame_->spatialOversample));
    form->addRow("Oversample", oversampleSpin_);

    filterRadiusSpin_ = new QDoubleSpinBox(this);
    filterRadiusSpin_->setObjectName("filterRadiusSpin");
    filterRadiusSpin_->setRange(0, 50);
    filterRadiusSpin_->setDecimals(2);
    filterRadiusSpin_->setSingleStep(0.1);
    filterRadiusSpin_->setValue(flame_->spatialFilterRadius);
    form->addRow("Filter Radius", filterRadiusSpin_);

    adaptiveFilterCheck_ = new QCheckBox(this);
    adaptiveFilterCheck_->setObjectName("adaptiveFilterCheck");
    adaptiveFilterCheck_->setChecked(flame_->enableDE);
    adaptiveFilterCheck_->setToolTip("Blur less in dense regions and more in sparse ones, reducing chaos-game "
                                      "noise without over-softening detail (Flame::estimator/estimatorMin/"
                                      "estimatorCurve, editable via the .flame file).");
    form->addRow("Adaptive Filter", adaptiveFilterCheck_);

    transparentBackgroundCheck_ = new QCheckBox(this);
    transparentBackgroundCheck_->setObjectName("transparentBackgroundCheck");
    transparentBackgroundCheck_->setChecked(flame_->transparency);
    transparentBackgroundCheck_->setToolTip(
        "Render with an alpha channel instead of filling empty areas with the background color - "
        "only takes effect in the output PNG (this dialog always renders to PNG).");
    form->addRow("Transparent Background", transparentBackgroundCheck_);

    memoryEstimateLabel_ = new QLabel(this);
    memoryEstimateLabel_->setObjectName("memoryEstimateLabel");
    form->addRow("Estimated Peak Memory", memoryEstimateLabel_);

    auto* outputRow = new QHBoxLayout();
    outputPathEdit_ = new QLineEdit(this);
    outputPathEdit_->setObjectName("outputPathEdit");
    outputPathEdit_->setPlaceholderText("Choose an output PNG file...");
    browseButton_ = new QPushButton("Browse...", this);
    browseButton_->setObjectName("browseButton");
    outputRow->addWidget(outputPathEdit_, 1);
    outputRow->addWidget(browseButton_);
    form->addRow("Output File", outputRow);

    saveParametersCheck_ = new QCheckBox("Save parameters (.flame) alongside the image", this);
    saveParametersCheck_->setObjectName("saveParametersCheck");
    form->addRow(saveParametersCheck_);

    rootLayout->addLayout(form);

    progressBar_ = new QProgressBar(this);
    progressBar_->setObjectName("progressBar");
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    rootLayout->addWidget(progressBar_);

    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setObjectName("statusLabel");
    rootLayout->addWidget(statusLabel_);

    auto* buttonRow = new QHBoxLayout();
    renderButton_ = new QPushButton("Render", this);
    renderButton_->setObjectName("renderButton");
    pauseButton_ = new QPushButton("Pause", this);
    pauseButton_->setObjectName("pauseButton");
    pauseButton_->setEnabled(false);
    cancelButton_ = new QPushButton("Cancel", this);
    cancelButton_->setObjectName("cancelButton");
    cancelButton_->setEnabled(false);
    buttonRow->addStretch(1);
    buttonRow->addWidget(renderButton_);
    buttonRow->addWidget(pauseButton_);
    buttonRow->addWidget(cancelButton_);
    rootLayout->addLayout(buttonRow);

    connect(maintainAspectCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        // aspectRatio_ is otherwise only ever set once, from the flame's
        // dimensions at dialog-open time - if the user typed a custom
        // width/height while this box was unchecked and only then checks
        // it, re-syncing off that stale construction-time ratio would
        // silently discard what they just typed. Recapture the ratio from
        // whatever's in the spin boxes right now instead.
        if (checked) aspectRatio_ = static_cast<double>(widthSpin_->value()) / std::max(1, heightSpin_->value());
    });
    connect(widthSpin_, &QSpinBox::valueChanged, this, [this](int w) {
        if (maintainAspectCheck_->isChecked() && widthSpin_->hasFocus()) {
            heightSpin_->blockSignals(true);
            heightSpin_->setValue(static_cast<int>(std::lround(w / aspectRatio_)));
            heightSpin_->blockSignals(false);
        }
    });
    connect(heightSpin_, &QSpinBox::valueChanged, this, [this](int h) {
        if (maintainAspectCheck_->isChecked() && heightSpin_->hasFocus()) {
            widthSpin_->blockSignals(true);
            widthSpin_->setValue(static_cast<int>(std::lround(h * aspectRatio_)));
            widthSpin_->blockSignals(false);
        }
    });
    // index 0 is the "Preset..." placeholder, not a real value. Focusing
    // the spin box first (rather than setValue()-ing it directly) means
    // picking a preset goes through the exact same hasFocus()-gated
    // maintain-aspect-ratio sync above as if the user had typed the value
    // in by hand - no separate sync logic to keep in step with that one.
    connect(widthPresetCombo_, &QComboBox::activated, this, [this](int index) {
        if (index <= 0) return;
        widthSpin_->setFocus();
        widthSpin_->setValue(widthPresetCombo_->currentText().toInt());
    });
    connect(heightPresetCombo_, &QComboBox::activated, this, [this](int index) {
        if (index <= 0) return;
        heightSpin_->setFocus();
        heightSpin_->setValue(heightPresetCombo_->currentText().toInt());
    });
    connect(densityPresetCombo_, &QComboBox::activated, this, [this](int index) {
        if (index <= 0) return;
        densitySpin_->setValue(densityPresetCombo_->currentText().toInt());
    });
    connect(outputPathEdit_, &QLineEdit::textChanged, this, &RenderDialog::updateRenderButtonEnabled);
    connect(browseButton_, &QPushButton::clicked, this, &RenderDialog::browseOutputPath);
    connect(renderButton_, &QPushButton::clicked, this, &RenderDialog::startRender);
    connect(pauseButton_, &QPushButton::clicked, this, &RenderDialog::togglePauseRender);
    connect(cancelButton_, &QPushButton::clicked, this, &RenderDialog::cancelRender);
    updateRenderButtonEnabled();

    // Only the controls that actually change bucket sizing - sample
    // density affects how many points get iterated, not how large the
    // bucket array is, so it's deliberately not connected here.
    connect(widthSpin_, &QSpinBox::valueChanged, this, &RenderDialog::updateMemoryEstimate);
    connect(heightSpin_, &QSpinBox::valueChanged, this, &RenderDialog::updateMemoryEstimate);
    connect(oversampleSpin_, &QSpinBox::valueChanged, this, &RenderDialog::updateMemoryEstimate);
    connect(filterRadiusSpin_, &QDoubleSpinBox::valueChanged, this, &RenderDialog::updateMemoryEstimate);
    updateMemoryEstimate();

    progressTimer_ = new QTimer(this);
    progressTimer_->setInterval(kProgressPollIntervalMs);
    connect(progressTimer_, &QTimer::timeout, this, &RenderDialog::onProgressTick);

    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &RenderDialog::fullRenderRequested, worker_, &RenderWorker::renderFull);
    connect(worker_, &RenderWorker::fullRenderFinished, this, &RenderDialog::onFullRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();
}

RenderDialog::~RenderDialog() {
    // A render still in flight when the dialog is closed must be stopped
    // before progress_ is destroyed - the worker thread holds a raw
    // pointer to it for the duration of the blocking render() call (see
    // RenderWorker::renderFull). Requesting cancellation first means the
    // worker observes it within one sub-batch and returns quickly, rather
    // than this destructor blocking for however long the full render
    // would otherwise have taken.
    if (progress_) progress_->cancelRequested.store(true, std::memory_order_relaxed);
    workerThread_->quit();
    workerThread_->wait();
}

void RenderDialog::setControlsEnabled(bool enabled) {
    widthSpin_->setEnabled(enabled);
    widthPresetCombo_->setEnabled(enabled);
    heightSpin_->setEnabled(enabled);
    heightPresetCombo_->setEnabled(enabled);
    maintainAspectCheck_->setEnabled(enabled);
    densitySpin_->setEnabled(enabled);
    densityPresetCombo_->setEnabled(enabled);
    oversampleSpin_->setEnabled(enabled);
    filterRadiusSpin_->setEnabled(enabled);
    adaptiveFilterCheck_->setEnabled(enabled);
    transparentBackgroundCheck_->setEnabled(enabled);
    outputPathEdit_->setEnabled(enabled);
    browseButton_->setEnabled(enabled);
    saveParametersCheck_->setEnabled(enabled);
    updateRenderButtonEnabled();
    pauseButton_->setEnabled(!enabled);
    // Reset for the next render - left as "Resume" here would be stale
    // (and wrong) the next time a render starts fresh and unpaused.
    if (enabled) pauseButton_->setText("Pause");
    cancelButton_->setEnabled(!enabled);
}

void RenderDialog::updateRenderButtonEnabled() {
    renderButton_->setEnabled(!fullRenderInFlight_ && !outputPathEdit_->text().isEmpty());
}

void RenderDialog::updateMemoryEstimate() {
    // A throwaway clone at exactly the dimensions a real render would use
    // (adjustScale, matching startRender()'s own construction) - cheap,
    // pure arithmetic (see Renderer::estimatePeakMemoryBytes's own doc
    // comment), safe to rebuild on every spin-box tick.
    auto probeFlame = flame_->clone();
    probeFlame->adjustScale(widthSpin_->value(), heightSpin_->value());
    probeFlame->spatialOversample = oversampleSpin_->value();
    probeFlame->spatialFilterRadius = filterRadiusSpin_->value();

    // B6: shows what will actually be used (after the memory-safety clamp),
    // not the raw AppSettings request - a label that quietly diverges from
    // startRender()'s own resolved thread count would be worse than no
    // estimate at all.
    const ThreadBudgetResult budget = resolveMemorySafeThreadCount(*probeFlame, AppSettings::renderThreadCount());
    QString text = QString("%1 (%2 thread%3)")
                        .arg(formatBytes(budget.estimatedBytes))
                        .arg(budget.threadCount)
                        .arg(budget.threadCount == 1 ? "" : "s");
    if (budget.clamped) text += " - reduced to fit available memory";
    memoryEstimateLabel_->setText(text);
}

void RenderDialog::browseOutputPath() {
    const QString path = QFileDialog::getSaveFileName(this, "Render Output", outputPathEdit_->text(),
                                                       "PNG Images (*.png)", nullptr, testFriendlyFileDialogOptions());
    if (!path.isEmpty()) outputPathEdit_->setText(path);
}

void RenderDialog::startRender() {
    if (fullRenderInFlight_) return;
    if (outputPathEdit_->text().isEmpty()) return;

    auto renderFlame = flame_->clone();
    renderFlame->adjustScale(widthSpin_->value(), heightSpin_->value());
    renderFlame->sampleDensity = densitySpin_->value();
    renderFlame->spatialOversample = oversampleSpin_->value();
    renderFlame->spatialFilterRadius = filterRadiusSpin_->value();
    renderFlame->enableDE = adaptiveFilterCheck_->isChecked();
    renderFlame->transparency = transparentBackgroundCheck_->isChecked();

    // B6: resolve the memory-safe thread count once here, from the exact
    // flame that's about to render - this is the value actually passed to
    // fullRenderRequested below, not the raw AppSettings request.
    const ThreadBudgetResult budget = resolveMemorySafeThreadCount(*renderFlame, AppSettings::renderThreadCount());

    progress_ = std::make_unique<apo::RenderProgress>();
    fullRenderInFlight_ = true;
    setControlsEnabled(false);
    progressBar_->setValue(0);
    statusLabel_->setText(budget.clamped ? QString("Using %1 thread%2 to stay under available memory...")
                                                .arg(budget.threadCount)
                                                .arg(budget.threadCount == 1 ? "" : "s")
                                          : "Rendering...");
    elapsedTimer_.start();
    progressTimer_->start();

    std::shared_ptr<const apo::Flame> shared(std::move(renderFlame));
    pendingRenderFlame_ = shared;
    const quint64 seed = static_cast<quint64>(std::random_device{}());
    emit fullRenderRequested(shared, seed, budget.threadCount, progress_.get(), outputPathEdit_->text());
}

void RenderDialog::cancelRender() {
    if (!progress_) return;
    // Setting cancelRequested alone is enough even if the render is
    // currently paused (blocked in waitWhilePaused()): that wait loop
    // rechecks cancelRequested itself and exits immediately once it's set,
    // without needing pauseRequested cleared first (see its own doc
    // comment) - so Cancel always works, whether or not Resume was clicked.
    progress_->cancelRequested.store(true, std::memory_order_relaxed);
    statusLabel_->setText("Cancelling...");
    pauseButton_->setEnabled(false);
    cancelButton_->setEnabled(false);
}

void RenderDialog::togglePauseRender() {
    if (!progress_) return;
    const bool nowPaused = !progress_->pauseRequested.load(std::memory_order_relaxed);
    progress_->pauseRequested.store(nowPaused, std::memory_order_relaxed);
    pauseButton_->setText(nowPaused ? "Resume" : "Pause");
    // Refresh the status text immediately rather than waiting for the next
    // progressTimer_ tick (up to kProgressPollIntervalMs away) - onProgressTick
    // itself already knows how to render the "Paused" state (see below), so
    // this just calls it early for a snappier-feeling button.
    onProgressTick();
}

void RenderDialog::onProgressTick() {
    if (!progress_) return;

    const std::uint64_t done = progress_->pointsDone.load(std::memory_order_relaxed);
    const std::uint64_t target = progress_->pointsTarget.load(std::memory_order_relaxed);
    const int percent = target > 0 ? static_cast<int>(std::min<std::uint64_t>(100, done * 100 / target)) : 0;
    progressBar_->setValue(percent);

    const bool paused = progress_->pauseRequested.load(std::memory_order_relaxed);
    const double elapsedSec = elapsedTimer_.elapsed() / 1000.0;
    statusLabel_->setText(QString("%1%2 / %3 points (%4%) - %5s elapsed")
                               .arg(paused ? "Paused - " : "")
                               .arg(done)
                               .arg(target)
                               .arg(percent)
                               .arg(elapsedSec, 0, 'f', 1));
}

void RenderDialog::onFullRenderFinished(QImage /*image*/, quint64 /*pointsGenerated*/, quint64 pointsAccepted,
                                         bool cancelled, bool saved) {
    progressTimer_->stop();
    progress_.reset();
    fullRenderInFlight_ = false;
    setControlsEnabled(true);

    const double elapsedSec = elapsedTimer_.elapsed() / 1000.0;
    if (cancelled) {
        progressBar_->setValue(0);
        statusLabel_->setText(QString("Cancelled after %1s").arg(elapsedSec, 0, 'f', 1));
    } else if (!saved) {
        progressBar_->setValue(100);
        statusLabel_->setText("Render finished, but failed to save the output file");
    } else {
        progressBar_->setValue(100);
        QString message = QString("Done - %1 points accepted in %2s").arg(pointsAccepted).arg(elapsedSec, 0, 'f', 1);

        // Plan's P4.3 - the exact settings that were actually rendered
        // (pendingRenderFlame_), not flame_'s own possibly-different
        // original values (width/density/oversample/... as this dialog's
        // own fields had them at Render time).
        if (saveParametersCheck_->isChecked() && pendingRenderFlame_) {
            const QString flamePath = flameParametersPathFor(outputPathEdit_->text());
            if (apo::saveFlameFile(flamePath.toStdString(), {pendingRenderFlame_.get()})) {
                message += " (parameters saved)";
            } else {
                message += " (failed to save parameters)";
            }
        }
        statusLabel_->setText(message);
    }
    pendingRenderFlame_.reset();
}

void RenderDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

void RenderDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    // Unlike AdjustDialog (which screenshots after its automatic preview
    // render settles), this dialog does nothing automatically - Render is
    // a deliberate user action, not something to trigger just for a CLI
    // screenshot test. "Shown" is this dialog's equivalent settled state.
    if (autoScreenshotFired_ || autoScreenshotPath_.isEmpty()) return;
    autoScreenshotFired_ = true;
    const QString path = autoScreenshotPath_;
    const bool exitAfter = autoScreenshotExit_;
    QTimer::singleShot(100, this, [this, path, exitAfter] {
        grab().save(path, "PNG");
        if (exitAfter) qApp->quit();
    });
}

} // namespace apo::ui
