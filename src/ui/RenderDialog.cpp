#include "RenderDialog.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <random>
#include <thread>

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "FileDialogSupport.h"
#include "MemoryBudget.h"
#include "PostProcessDialog.h"
#include "RenderWorker.h"
#include "TimeFormat.h"
#include "WindowGeometry.h"
#include "core/io/FlameIO.h"
#include "core/render/RenderDispatcher.h"

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

    fasterRenderingCheck_ = new QCheckBox(this);
    fasterRenderingCheck_->setObjectName("fasterRenderingCheck");
    fasterRenderingCheck_->setChecked(false);
    fasterRenderingCheck_->setToolTip(
        "Render with float-precision histogram buckets instead of double - roughly 2x faster on the GPU backend "
        "for a typical flame (measured; less on the CPU backend, and negligible for blur-heavy variations that "
        "are already RNG/math-bound rather than memory/atomic-bound), at two costs: a bucket's point count "
        "saturates past ~16.7 million points (a real risk only at extreme density on a small/detailed canvas), "
        "and color channels round slightly differently than the Double baseline. Leave unchecked for the exact "
        "output this dialog has always produced.");
    form->addRow("Faster Rendering", fasterRenderingCheck_);

    backendCombo_ = new QComboBox(this);
    backendCombo_->setObjectName("backendCombo");
    backendCombo_->addItem("Auto (measure this flame and pick)", static_cast<int>(apo::RenderBackend::Auto));
    backendCombo_->addItem("GPU + CPU", static_cast<int>(apo::RenderBackend::Hybrid));
    backendCombo_->addItem("GPU only", static_cast<int>(apo::RenderBackend::Gpu));
    backendCombo_->addItem("CPU only", static_cast<int>(apo::RenderBackend::Cpu));
    backendCombo_->setToolTip(
        "Auto briefly renders this flame on the GPU and the CPU at these settings and picks the faster setup: "
        "GPU + CPU when the CPU can contribute a good share of the points (flames whose xforms use many "
        "different variations), GPU only when the GPU is much faster on its own.");
    {
        const int saved = backendCombo_->findData(AppSettings::renderBackendChoice());
        backendCombo_->setCurrentIndex(saved >= 0 ? saved : 0);
    }
    form->addRow("Render Backend", backendCombo_);

    backendIndicatorLabel_ = new QLabel(this);
    backendIndicatorLabel_->setObjectName("backendIndicatorLabel");
    backendIndicatorLabel_->setWordWrap(true);
    form->addRow("", backendIndicatorLabel_);

    memoryEstimateLabel_ = new QLabel(this);
    memoryEstimateLabel_->setObjectName("memoryEstimateLabel");
    form->addRow("Estimated Peak Memory (CPU)", memoryEstimateLabel_);

    gpuMemoryEstimateLabel_ = new QLabel(this);
    gpuMemoryEstimateLabel_->setObjectName("gpuMemoryEstimateLabel");
    form->addRow("Estimated Peak Memory (GPU)", gpuMemoryEstimateLabel_);

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
    postProcessButton_ = new QPushButton("Post Process...", this);
    postProcessButton_->setObjectName("postProcessButton");
    postProcessButton_->setToolTip("Tune tone-mapping (gamma/brightness/contrast/vibrancy/background) on the "
                                    "image that was just rendered, without changing this flame's own settings");
    postProcessButton_->setEnabled(false);
    buttonRow->addWidget(postProcessButton_);
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
    connect(postProcessButton_, &QPushButton::clicked, this, &RenderDialog::openPostProcess);
    updateRenderButtonEnabled();

    // Only the controls that actually change bucket sizing - sample
    // density affects how many points get iterated, not how large the
    // bucket array is, so it's deliberately not connected here.
    connect(widthSpin_, &QSpinBox::valueChanged, this, &RenderDialog::updateMemoryEstimate);
    connect(heightSpin_, &QSpinBox::valueChanged, this, &RenderDialog::updateMemoryEstimate);
    connect(oversampleSpin_, &QSpinBox::valueChanged, this, &RenderDialog::updateMemoryEstimate);
    connect(filterRadiusSpin_, &QDoubleSpinBox::valueChanged, this, &RenderDialog::updateMemoryEstimate);
    // Density doesn't change memory, but it does change whether the render
    // clears RenderDispatcher::kMinGpuBatches - the backend label needs it.
    connect(densitySpin_, &QDoubleSpinBox::valueChanged, this, &RenderDialog::updateMemoryEstimate);
    connect(fasterRenderingCheck_, &QCheckBox::toggled, this, &RenderDialog::updateMemoryEstimate);
    connect(backendCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        AppSettings::setRenderBackendChoice(backendCombo_->currentData().toInt());
        updateMemoryEstimate();
    });
    probeTimer_ = new QTimer(this);
    probeTimer_->setSingleShot(true);
    probeTimer_->setInterval(600);
    connect(probeTimer_, &QTimer::timeout, this, &RenderDialog::startProbe);
    updateMemoryEstimate();

    progressTimer_ = new QTimer(this);
    progressTimer_->setInterval(kProgressPollIntervalMs);
    connect(progressTimer_, &QTimer::timeout, this, &RenderDialog::onProgressTick);

    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &RenderDialog::fullRenderRequested, worker_, &RenderWorker::renderFullWithBackend);
    connect(worker_, &RenderWorker::fullRenderFinished, this, &RenderDialog::onFullRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();

    // See WindowGeometry.h's restoreWindowGeometry() doc comment: must come
    // after the full UI/layout is built, not right after the resize() above.
    restoreWindowGeometry(this, "RenderDialog");
}

RenderDialog::~RenderDialog() {
    // A render still in flight when the dialog is closed must be stopped
    // before progress_ is destroyed - the worker thread holds a raw
    // pointer to it for the duration of the blocking render() call (see
    // RenderWorker::renderFull). Requesting cancellation first means the
    // worker observes it within one sub-batch and returns quickly, rather
    // than this destructor blocking for however long the full render
    // would otherwise have taken.
    abortProbe();
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
    fasterRenderingCheck_->setEnabled(enabled);
    backendCombo_->setEnabled(enabled);
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
    probeFlame->sampleDensity = densitySpin_->value();
    probeFlame->spatialOversample = oversampleSpin_->value();
    probeFlame->spatialFilterRadius = filterRadiusSpin_->value();

    // Mirrors fasterRenderingCheck_'s state - matches both estimates below
    // to what startRender() would actually allocate right now.
    const apo::BucketPrecision precision =
        fasterRenderingCheck_->isChecked() ? apo::BucketPrecision::Float : apo::BucketPrecision::Double;

    // B6: shows what will actually be used (after the memory-safety clamp),
    // not the raw AppSettings request - a label that quietly diverges from
    // startRender()'s own resolved thread count would be worse than no
    // estimate at all.
    const ThreadBudgetResult budget =
        resolveMemorySafeThreadCount(*probeFlame, AppSettings::renderThreadCount(), precision);
    QString text = QString("%1 (%2 thread%3)")
                        .arg(formatBytes(budget.estimatedBytes))
                        .arg(budget.threadCount)
                        .arg(budget.threadCount == 1 ? "" : "s");
    if (budget.clamped) text += " - reduced to fit available memory";
    memoryEstimateLabel_->setText(text);

    const std::uint64_t gpuBytes = apo::RenderDispatcher::estimateGpuPeakMemoryBytes(*probeFlame, precision);
    gpuMemoryEstimateLabel_->setText(gpuBytes > 0 ? formatBytes(gpuBytes) : "-");

    updateBackendLabel(*probeFlame, budget.threadCount);
    scheduleProbe();
}

void RenderDialog::browseOutputPath() {
    QString start = outputPathEdit_->text();
    if (start.isEmpty()) {
        const QString lastDir = AppSettings::lastRenderOutputDirectory();
        if (!lastDir.isEmpty()) start = QDir(lastDir).filePath("untitled.png");
    }
    const QString path = QFileDialog::getSaveFileName(this, "Render Output", start, "PNG Images (*.png)", nullptr,
                                                       testFriendlyFileDialogOptions());
    if (path.isEmpty()) return;
    outputPathEdit_->setText(ensureFileSuffix(path, ".png"));
    AppSettings::setLastRenderOutputDirectory(QFileInfo(path).absolutePath());
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

    const apo::BucketPrecision precision =
        fasterRenderingCheck_->isChecked() ? apo::BucketPrecision::Float : apo::BucketPrecision::Double;

    // B6: resolve the memory-safe thread count once here, from the exact
    // flame that's about to render - this is the value actually passed to
    // fullRenderRequested below, not the raw AppSettings request.
    const ThreadBudgetResult budget =
        resolveMemorySafeThreadCount(*renderFlame, AppSettings::renderThreadCount(), precision);

    // A probe would compete with the render for the CPU and GPU.
    abortProbe();
    const apo::RenderBackend backend = resolvedBackend(*renderFlame, budget.threadCount);
    renderBackend_ = backend;
    renderEstimateRaw_ =
        backend == apo::RenderBackend::Auto ? 0.0 : estimatedSeconds(*renderFlame, backend, /*corrected=*/false);

    progress_ = std::make_unique<apo::RenderProgress>();
    fullRenderInFlight_ = true;
    setControlsEnabled(false);
    // Disable rather than leave pointing at whatever finished last - a
    // fresh render invalidates it until this one finishes successfully too.
    postProcessButton_->setEnabled(false);
    lastRenderedFlame_.reset();
    progressBar_->setValue(0);
    statusLabel_->setText(budget.clamped ? QString("Using %1 thread%2 to stay under available memory...")
                                                .arg(budget.threadCount)
                                                .arg(budget.threadCount == 1 ? "" : "s")
                                          : "Rendering...");
    elapsedTimer_.start();
    pausedMs_ = 0;
    pauseTimer_.invalidate();
    progressTimer_->start();

    std::shared_ptr<const apo::Flame> shared(std::move(renderFlame));
    pendingRenderFlame_ = shared;

    // Plan's P4.3 - save the .flame *before* the render starts (rather than
    // only after it finishes) so a crash mid-render still leaves the user
    // with the exact parameters to re-run from, instead of losing them
    // along with the incomplete render.
    //
    // Saves flame_ (the flame exactly as the editor/library holds it), not
    // the render copy: reopening the file must show the same framing the
    // editor showed, so the render dialog's own size (adjustScale) and
    // quality/filter/DE/transparency overrides are deliberately not baked
    // into it - those get re-entered here at the next render.
    parametersSaved_ = false;
    if (saveParametersCheck_->isChecked()) {
        const QString flamePath = flameParametersPathFor(outputPathEdit_->text());
        parametersSaved_ = apo::saveFlameFile(flamePath.toStdString(), {flame_.get()});
    }

    const quint64 seed = static_cast<quint64>(std::random_device{}());
    emit fullRenderRequested(shared, seed, budget.threadCount, progress_.get(), outputPathEdit_->text(), precision,
                             static_cast<int>(backend));
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
    if (nowPaused) {
        pauseTimer_.start();
    } else if (pauseTimer_.isValid()) {
        pausedMs_ += pauseTimer_.elapsed();
        pauseTimer_.invalidate();
    }
    // Refresh the status text immediately rather than waiting for the next
    // progressTimer_ tick (up to kProgressPollIntervalMs away) - onProgressTick
    // itself already knows how to render the "Paused" state (see below), so
    // this just calls it early for a snappier-feeling button.
    onProgressTick();
}

double RenderDialog::activeElapsedSec() const {
    qint64 ms = elapsedTimer_.elapsed() - pausedMs_;
    if (pauseTimer_.isValid()) ms -= pauseTimer_.elapsed();
    return std::max<qint64>(0, ms) / 1000.0;
}

void RenderDialog::onProgressTick() {
    if (!progress_) return;

    const std::uint64_t done = progress_->pointsDone.load(std::memory_order_relaxed);
    const std::uint64_t target = progress_->pointsTarget.load(std::memory_order_relaxed);
    const int percent = target > 0 ? static_cast<int>(std::min<std::uint64_t>(100, done * 100 / target)) : 0;
    progressBar_->setValue(percent);

    const bool paused = progress_->pauseRequested.load(std::memory_order_relaxed);
    const double elapsedSec = activeElapsedSec();
    // ETA is a simple linear extrapolation from progress-so-far - no
    // point-rate smoothing, since sample density (and thus points/sec) is
    // constant for the duration of a single render, so the estimate only
    // gets more accurate as `done` grows, never noisier.
    QString etaText;
    if (paused) {
        etaText = "paused";
    } else if (done >= target) {
        etaText = formatDuration(0.0);
    } else if (done > 0) {
        etaText =
            formatDuration(elapsedSec * static_cast<double>(target - done) / static_cast<double>(done));
    } else {
        etaText = "calculating...";
    }
    statusLabel_->setText(QString("%1%2 / %3 points (%4%) - %5 elapsed, %6 remaining")
                               .arg(paused ? "Paused - " : "")
                               .arg(done)
                               .arg(target)
                               .arg(percent)
                               .arg(formatDuration(elapsedSec))
                               .arg(etaText));
}

void RenderDialog::onFullRenderFinished(QImage /*image*/, quint64 /*pointsGenerated*/, quint64 pointsAccepted,
                                         bool cancelled, bool saved, bool usedGpu) {
    progressTimer_->stop();
    progress_.reset();
    fullRenderInFlight_ = false;
    setControlsEnabled(true);

    const QString backendSuffix = usedGpu ? " [GPU]" : "";
    const double elapsedSec = activeElapsedSec();

    // Teach the pre-render estimate how this machine really does on long
    // renders (see AppSettings::renderTimeCorrection). Short renders say
    // little about sustained thermal limits, and a render that fell back
    // to a different backend than the estimate was for says nothing.
    constexpr double kMinSecondsToLearn = 20.0;
    if (!cancelled && saved && renderEstimateRaw_ > 0 && elapsedSec >= kMinSecondsToLearn &&
        usedGpu == (renderBackend_ != apo::RenderBackend::Cpu)) {
        AppSettings::recordRenderTimeCorrection(static_cast<int>(renderBackend_), elapsedSec / renderEstimateRaw_);
    }
    renderEstimateRaw_ = 0;
    // The .flame was already written (or attempted) up front in
    // startRender() - just report what happened, rather than re-saving here
    // (which would also be wrong now, since a cancelled/crashed render has
    // no guarantee pendingRenderFlame_'s settings still reflect anything
    // worth re-saving over the already-written file).
    const QString paramsSuffix = saveParametersCheck_->isChecked()
        ? (parametersSaved_ ? " (parameters saved)" : " (failed to save parameters)")
        : "";
    if (cancelled) {
        progressBar_->setValue(0);
        statusLabel_->setText(
            QString("Cancelled after %1%2%3").arg(formatDuration(elapsedSec)).arg(backendSuffix).arg(paramsSuffix));
    } else if (!saved) {
        progressBar_->setValue(100);
        statusLabel_->setText(QString("Render finished, but failed to save the output file%1").arg(paramsSuffix));
    } else {
        progressBar_->setValue(100);
        const QString message = QString("Done - %1 points accepted in %2%3%4")
                                     .arg(pointsAccepted)
                                     .arg(formatDuration(elapsedSec))
                                     .arg(backendSuffix)
                                     .arg(paramsSuffix);
        statusLabel_->setText(message);

        // Matches the original's Render > post-process flow (only offered
        // once a render actually completed and its image was saved) -
        // captured here, before pendingRenderFlame_ is reset below, so
        // openPostProcess() can seed PostProcessDialog with exactly what
        // was just rendered.
        lastRenderedFlame_ = pendingRenderFlame_;
        postProcessButton_->setEnabled(true);
    }
    pendingRenderFlame_.reset();
}

void RenderDialog::openPostProcess() {
    if (!lastRenderedFlame_) return;
    // Fresh dialog per click (WA_DeleteOnClose) - same fire-and-forget
    // pattern EditorWindow used for this dialog.
    auto* dialog = new PostProcessDialog(lastRenderedFlame_, this);
    dialog->show();
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
    // Waits (up to 15s) for the Auto backend probe to settle, so the
    // screenshot shows its result rather than "measuring...".
    auto* poll = new QTimer(this);
    poll->setInterval(100);
    auto waited = std::make_shared<QElapsedTimer>();
    waited->start();
    connect(poll, &QTimer::timeout, this, [this, poll, waited, path, exitAfter] {
        const bool probeBusy = probeRunning_ || (probeTimer_ && probeTimer_->isActive());
        if (probeBusy && waited->elapsed() < 15000) return;
        poll->stop();
        poll->deleteLater();
        grab().save(path, "PNG");
        if (exitAfter) qApp->quit();
    });
    poll->start();
}

void RenderDialog::closeEvent(QCloseEvent* event) {
    saveWindowGeometry(this, "RenderDialog");
    QDialog::closeEvent(event);
}

QString RenderDialog::currentProbeKey() const {
    return QString("%1x%2 os%3 f%4 %5 t%6 g%7")
        .arg(widthSpin_->value())
        .arg(heightSpin_->value())
        .arg(oversampleSpin_->value())
        .arg(filterRadiusSpin_->value())
        .arg(fasterRenderingCheck_->isChecked() ? "float" : "double")
        .arg(AppSettings::renderThreadCount())
        .arg(AppSettings::useGpuRendering() ? 1 : 0);
}

void RenderDialog::scheduleProbe() {
    if (!probeTimer_ || fullRenderInFlight_) return;
    const QString key = currentProbeKey();
    if ((probeValid_ && key == probeKey_) || key == probeFailedKey_) {
        probeTimer_->stop();
        return;
    }
    probeTimer_->start(); // restarts the debounce on every edit
}

void RenderDialog::startProbe() {
    if (probeRunning_ || fullRenderInFlight_) return;
    const QString key = currentProbeKey();
    if ((probeValid_ && key == probeKey_) || key == probeFailedKey_) return;

    auto probeFlame = flame_->clone();
    probeFlame->adjustScale(widthSpin_->value(), heightSpin_->value());
    probeFlame->sampleDensity = densitySpin_->value();
    probeFlame->spatialOversample = oversampleSpin_->value();
    probeFlame->spatialFilterRadius = filterRadiusSpin_->value();
    const apo::BucketPrecision precision =
        fasterRenderingCheck_->isChecked() ? apo::BucketPrecision::Float : apo::BucketPrecision::Double;
    // Measured at the full requested thread count (hybrid's CPU side isn't
    // memory-limited), then the CPU-only estimate is scaled to the
    // memory-safe count a CPU-only render would actually get.
    const int requestedThreads = AppSettings::renderThreadCount();
    const int cpuOnlyThreads = resolveMemorySafeThreadCount(*probeFlame, requestedThreads, precision).threadCount;
    const bool preferGpu = AppSettings::useGpuRendering();

    if (probeThread_.joinable()) probeThread_.join(); // already finished
    probeAbort_.store(false);
    probeRunning_ = true;
    std::shared_ptr<const apo::Flame> shared(std::move(probeFlame));
    probeThread_ = std::thread([this, shared, key, requestedThreads, cpuOnlyThreads, precision, preferGpu] {
        apo::BackendProbeResult result;
        try {
            result = apo::probeBackends(*shared, requestedThreads, precision, preferGpu, &probeAbort_);
            if (result.valid) result.threadCount = cpuOnlyThreads;
        } catch (...) {
            result = {};
        }
        const bool aborted = probeAbort_.load();
        // Queued to the UI thread; dropped by Qt if the dialog is gone (its
        // destructor joins this thread first, via abortProbe()).
        QMetaObject::invokeMethod(
            this, [this, key, result, aborted] { onProbeFinished(key, result, aborted); }, Qt::QueuedConnection);
    });
    updateMemoryEstimate(); // "measuring..."
}

void RenderDialog::onProbeFinished(const QString& key, const apo::BackendProbeResult& result, bool aborted) {
    if (aborted) return; // abortProbe() already reset the state
    probeRunning_ = false;
    if (result.valid) {
        probeResult_ = result;
        probeKey_ = key;
        probeValid_ = true;
    } else {
        probeFailedKey_ = key;
    }
    if (!fullRenderInFlight_) updateMemoryEstimate(); // new label; re-probes if settings moved on meanwhile
}

void RenderDialog::abortProbe() {
    if (probeTimer_) probeTimer_->stop();
    if (probeThread_.joinable()) {
        probeAbort_.store(true);
        probeThread_.join();
        probeAbort_.store(false);
    }
    probeRunning_ = false;
}

apo::RenderBackend RenderDialog::resolvedBackend(const apo::Flame& /*renderFlame*/, int /*threadCount*/) const {
    const auto choice = static_cast<apo::RenderBackend>(backendCombo_->currentData().toInt());
    if (choice != apo::RenderBackend::Auto) return choice;
    if (probeValid_ && probeKey_ == currentProbeKey()) return apo::recommendBackend(probeResult_);
    return apo::RenderBackend::Auto;
}

double RenderDialog::estimatedSeconds(const apo::Flame& renderFlame, apo::RenderBackend backend,
                                      bool corrected) const {
    if (backend == apo::RenderBackend::Auto || !probeValid_ || probeKey_ != currentProbeKey()) return 0;
    double secs = apo::estimateRenderSeconds(probeResult_, backend, apo::plannedPointCount(renderFlame));
    if (corrected) secs *= AppSettings::renderTimeCorrection(static_cast<int>(backend));
    return secs;
}

void RenderDialog::updateBackendLabel(const apo::Flame& probeFlame, int threadCount) {
    // Whether the GPU can take this render at all (CUDA present, no plugin
    // variations, large enough - see RenderDispatcher::wouldUseGpu); the
    // GPU choices are greyed out when not. Auto additionally respects the
    // Options "use GPU" setting, as every other render path does.
    const bool gpuPossible = apo::RenderDispatcher::wouldUseGpu(probeFlame, /*preferGpu=*/true);
    if (auto* model = qobject_cast<QStandardItemModel*>(backendCombo_->model())) {
        for (int i = 0; i < backendCombo_->count(); ++i) {
            const auto b = static_cast<apo::RenderBackend>(backendCombo_->itemData(i).toInt());
            if (b == apo::RenderBackend::Gpu || b == apo::RenderBackend::Hybrid) {
                if (QStandardItem* item = model->item(i)) item->setEnabled(gpuPossible);
            }
        }
    }

    const auto choice = static_cast<apo::RenderBackend>(backendCombo_->currentData().toInt());
    const bool autoChoice = choice == apo::RenderBackend::Auto;
    const bool gpuForChoice =
        apo::RenderDispatcher::wouldUseGpu(probeFlame, autoChoice ? AppSettings::useGpuRendering() : true);
    apo::RenderBackend shown = choice;
    if (autoChoice) {
        shown = gpuForChoice ? resolvedBackend(probeFlame, threadCount) : apo::RenderBackend::Cpu;
    } else if (choice != apo::RenderBackend::Cpu && !gpuForChoice) {
        shown = apo::RenderBackend::Cpu; // RenderDispatcher will fall back
    }

    QString text = autoChoice ? "Auto: " : "";
    switch (shown) {
        case apo::RenderBackend::Auto:
            text += "measuring this flame...";
            break;
        case apo::RenderBackend::Hybrid: {
            // Not threadCount: that's reduced to bound the CPU renderer's
            // per-thread histograms, which hybrid's CPU workers don't keep
            // (RenderWorker::renderFullWithBackend passes the same count).
            const int cpuThreads = apo::RenderDispatcher::cpuAssistThreadCount(AppSettings::renderThreadCount());
            text += QString("GPU (CUDA) + %1 CPU thread%2").arg(cpuThreads).arg(cpuThreads == 1 ? "" : "s");
            break;
        }
        case apo::RenderBackend::Gpu:
            text += "GPU (CUDA)";
            break;
        case apo::RenderBackend::Cpu:
            if (gpuForChoice) {
                text += QString("CPU (%1 thread%2)").arg(threadCount).arg(threadCount == 1 ? "" : "s");
            } else {
                text += QString("CPU - %1").arg(QString::fromStdString(apo::RenderDispatcher::cpuFallbackReason(
                    probeFlame, autoChoice ? AppSettings::useGpuRendering() : true)));
            }
            break;
    }
    const double secs = estimatedSeconds(probeFlame, shown, /*corrected=*/true);
    if (secs >= 1) {
        text += QString(" - about %1").arg(formatDuration(secs));
    } else if (secs > 0) {
        text += " - about a second or less";
    }
    backendIndicatorLabel_->setText(text);

    if (probeValid_ && probeKey_ == currentProbeKey()) {
        const QString gpuPart = probeResult_.gpuPointsPerSec > 0
            ? QString("GPU %1M").arg(probeResult_.gpuPointsPerSec / 1e6, 0, 'f', 1)
            : QString("GPU not used");
        backendIndicatorLabel_->setToolTip(QString("Measured on this flame at these settings: %1, CPU %2M points/s "
                                                   "(%3 threads). Estimates get more accurate after a few long "
                                                   "renders.")
                                               .arg(gpuPart)
                                               .arg(probeResult_.cpuPointsPerSec / 1e6, 0, 'f', 1)
                                               .arg(probeResult_.cpuThreads));
    } else {
        backendIndicatorLabel_->setToolTip({});
    }
}

} // namespace apo::ui
