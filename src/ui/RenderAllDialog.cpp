#include "RenderAllDialog.h"

#include <algorithm>
#include <cstddef>
#include <random>

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "MemoryBudget.h"
#include "RenderWorker.h"
#include "core/io/FlameIO.h"

namespace apo::ui {

namespace {
// Same poll cadence as RenderDialog's own progress timer.
constexpr int kProgressPollIntervalMs = 150;

// Strips characters Windows (and, for simplicity, every other target)
// rejects in a path component, and falls back to a non-empty placeholder -
// a flame's `name` is free-form user text (e.g. "3D / spirals??"), but a
// filename can't be.
QString sanitizeFileNameBase(const QString& name) {
    QString result = name.trimmed();
    static const QString kInvalidChars = QStringLiteral("<>:\"/\\|?*");
    for (QChar& ch : result) {
        if (kInvalidChars.contains(ch) || ch.unicode() < 0x20) ch = QChar('_');
    }
    while (!result.isEmpty() && (result.endsWith('.') || result.endsWith(' '))) result.chop(1);
    if (result.isEmpty()) result = "flame";
    return result;
}

// One ".png" filename per flame, computed once up front so a same-named
// (or unnamed) pair of library entries doesn't have the second silently
// overwrite the first's output - "name", "name (2)", "name (3)", ...
std::vector<QString> computeOutputFileNames(const std::vector<std::shared_ptr<apo::Flame>>& flames) {
    std::vector<QString> result;
    result.reserve(flames.size());
    QHash<QString, int> seenCount;
    for (const auto& flame : flames) {
        const QString base = sanitizeFileNameBase(QString::fromStdString(flame->name));
        int& count = seenCount[base];
        const QString candidate = count == 0 ? base : QString("%1 (%2)").arg(base).arg(count + 1);
        ++count;
        result.push_back(candidate + ".png");
    }
    return result;
}
} // namespace

RenderAllDialog::RenderAllDialog(std::vector<std::shared_ptr<apo::Flame>> flames, QWidget* parent)
    : QDialog(parent), flames_(std::move(flames)) {
    setWindowTitle(QString("Render All Flames (%1)").arg(flames_.size()));
    resize(460, 520);
    setAttribute(Qt::WA_DeleteOnClose);

    outputFileNames_ = computeOutputFileNames(flames_);

    auto* rootLayout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    // Every shared setting starts from the first flame's own values (an
    // arbitrary but reasonable "most likely representative" starting
    // point, same spirit as RenderDialog seeding its controls from the one
    // flame it renders) - a fresh, empty-library-safe default (640x480,
    // etc.) if there's nothing to seed from.
    const int defaultWidth = flames_.empty() ? 640 : flames_.front()->width;
    const int defaultHeight = flames_.empty() ? 480 : flames_.front()->height;
    const double defaultDensity = flames_.empty() ? 200.0 : flames_.front()->sampleDensity;
    const int defaultOversample = flames_.empty() ? 1 : std::max(1, flames_.front()->spatialOversample);
    const double defaultFilterRadius = flames_.empty() ? 0.2 : flames_.front()->spatialFilterRadius;
    const bool defaultAdaptiveFilter = !flames_.empty() && flames_.front()->enableDE;
    const bool defaultTransparency = !flames_.empty() && flames_.front()->transparency;

    widthSpin_ = new QSpinBox(this);
    widthSpin_->setObjectName("widthSpin");
    widthSpin_->setRange(1, 16384);
    widthSpin_->setValue(defaultWidth);
    form->addRow("Width", widthSpin_);

    heightSpin_ = new QSpinBox(this);
    heightSpin_->setObjectName("heightSpin");
    heightSpin_->setRange(1, 16384);
    heightSpin_->setValue(defaultHeight);
    form->addRow("Height", heightSpin_);

    densitySpin_ = new QDoubleSpinBox(this);
    densitySpin_->setObjectName("densitySpin");
    densitySpin_->setRange(1, 1000000);
    densitySpin_->setDecimals(1);
    densitySpin_->setValue(defaultDensity);
    form->addRow("Sample Density", densitySpin_);

    oversampleSpin_ = new QSpinBox(this);
    oversampleSpin_->setObjectName("oversampleSpin");
    oversampleSpin_->setRange(1, 8);
    oversampleSpin_->setValue(defaultOversample);
    form->addRow("Oversample", oversampleSpin_);

    filterRadiusSpin_ = new QDoubleSpinBox(this);
    filterRadiusSpin_->setObjectName("filterRadiusSpin");
    filterRadiusSpin_->setRange(0, 50);
    filterRadiusSpin_->setDecimals(2);
    filterRadiusSpin_->setSingleStep(0.1);
    filterRadiusSpin_->setValue(defaultFilterRadius);
    form->addRow("Filter Radius", filterRadiusSpin_);

    adaptiveFilterCheck_ = new QCheckBox(this);
    adaptiveFilterCheck_->setObjectName("adaptiveFilterCheck");
    adaptiveFilterCheck_->setChecked(defaultAdaptiveFilter);
    form->addRow("Adaptive Filter", adaptiveFilterCheck_);

    transparentBackgroundCheck_ = new QCheckBox(this);
    transparentBackgroundCheck_->setObjectName("transparentBackgroundCheck");
    transparentBackgroundCheck_->setChecked(defaultTransparency);
    form->addRow("Transparent Background", transparentBackgroundCheck_);

    auto* outputRow = new QHBoxLayout();
    outputFolderEdit_ = new QLineEdit(this);
    outputFolderEdit_->setObjectName("outputFolderEdit");
    outputFolderEdit_->setPlaceholderText("Choose an output folder...");
    browseButton_ = new QPushButton("Browse...", this);
    browseButton_->setObjectName("browseButton");
    outputRow->addWidget(outputFolderEdit_, 1);
    outputRow->addWidget(browseButton_);
    form->addRow("Output Folder", outputRow);

    saveParametersCheck_ = new QCheckBox("Save parameters (.flame) alongside each image", this);
    saveParametersCheck_->setObjectName("saveParametersCheck");
    form->addRow(saveParametersCheck_);

    rootLayout->addLayout(form);

    resultsList_ = new QListWidget(this);
    resultsList_->setObjectName("resultsList");
    for (std::size_t i = 0; i < flames_.size(); ++i) {
        const QString name = flames_[i]->name.empty() ? QString("Flame %1").arg(i + 1)
                                                        : QString::fromStdString(flames_[i]->name);
        resultsList_->addItem(QString("%1 -> %2").arg(name, outputFileNames_[i]));
    }
    rootLayout->addWidget(resultsList_, 1);

    overallLabel_ = new QLabel(
        flames_.empty() ? "No flames to render"
                         : QString("Ready to render %1 flame%2").arg(flames_.size()).arg(flames_.size() == 1 ? "" : "s"),
        this);
    overallLabel_->setObjectName("overallLabel");
    rootLayout->addWidget(overallLabel_);

    progressBar_ = new QProgressBar(this);
    progressBar_->setObjectName("progressBar");
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    rootLayout->addWidget(progressBar_);

    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setObjectName("statusLabel");
    rootLayout->addWidget(statusLabel_);

    auto* buttonRow = new QHBoxLayout();
    startButton_ = new QPushButton("Render All", this);
    startButton_->setObjectName("startButton");
    cancelButton_ = new QPushButton("Cancel", this);
    cancelButton_->setObjectName("cancelButton");
    cancelButton_->setEnabled(false);
    buttonRow->addStretch(1);
    buttonRow->addWidget(startButton_);
    buttonRow->addWidget(cancelButton_);
    rootLayout->addLayout(buttonRow);

    connect(outputFolderEdit_, &QLineEdit::textChanged, this, &RenderAllDialog::updateStartButtonEnabled);
    connect(browseButton_, &QPushButton::clicked, this, &RenderAllDialog::browseOutputFolder);
    connect(startButton_, &QPushButton::clicked, this, &RenderAllDialog::startRenderAll);
    connect(cancelButton_, &QPushButton::clicked, this, &RenderAllDialog::cancelRenderAll);
    updateStartButtonEnabled();

    progressTimer_ = new QTimer(this);
    progressTimer_->setInterval(kProgressPollIntervalMs);
    connect(progressTimer_, &QTimer::timeout, this, &RenderAllDialog::onProgressTick);

    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &RenderAllDialog::fullRenderRequested, worker_, &RenderWorker::renderFull);
    connect(worker_, &RenderWorker::fullRenderFinished, this, &RenderAllDialog::onFullRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();
}

RenderAllDialog::~RenderAllDialog() {
    // Same reasoning as RenderDialog's destructor - stop an in-flight
    // render before progress_ is destroyed out from under the worker
    // thread's still-running render() call.
    if (progress_) progress_->cancelRequested.store(true, std::memory_order_relaxed);
    workerThread_->quit();
    workerThread_->wait();
}

void RenderAllDialog::setControlsEnabled(bool enabled) {
    widthSpin_->setEnabled(enabled);
    heightSpin_->setEnabled(enabled);
    densitySpin_->setEnabled(enabled);
    oversampleSpin_->setEnabled(enabled);
    filterRadiusSpin_->setEnabled(enabled);
    adaptiveFilterCheck_->setEnabled(enabled);
    transparentBackgroundCheck_->setEnabled(enabled);
    outputFolderEdit_->setEnabled(enabled);
    browseButton_->setEnabled(enabled);
    saveParametersCheck_->setEnabled(enabled);
    updateStartButtonEnabled();
    cancelButton_->setEnabled(!enabled);
}

void RenderAllDialog::updateStartButtonEnabled() {
    startButton_->setEnabled(!batchInFlight_ && !flames_.empty() && !outputFolderEdit_->text().isEmpty());
}

void RenderAllDialog::browseOutputFolder() {
    const QString path = QFileDialog::getExistingDirectory(this, "Render All - Output Folder", outputFolderEdit_->text());
    if (!path.isEmpty()) outputFolderEdit_->setText(path);
}

void RenderAllDialog::startRenderAll() {
    if (batchInFlight_ || flames_.empty() || outputFolderEdit_->text().isEmpty()) return;

    batchInFlight_ = true;
    currentIndex_ = 0;
    succeededCount_ = 0;
    failedCount_ = 0;
    cancelledMidBatch_ = false;
    setControlsEnabled(false);
    statusLabel_->setText("Starting...");
    renderNext();
}

void RenderAllDialog::cancelRenderAll() {
    if (!progress_) return;
    progress_->cancelRequested.store(true, std::memory_order_relaxed);
    statusLabel_->setText("Cancelling...");
    cancelButton_->setEnabled(false);
}

void RenderAllDialog::renderNext() {
    if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(flames_.size())) {
        batchInFlight_ = false;
        currentIndex_ = -1;
        setControlsEnabled(true);
        progressTimer_->stop();

        const int notStarted = static_cast<int>(flames_.size()) - succeededCount_ - failedCount_;
        const QString summary = cancelledMidBatch_
            ? QString("Cancelled - %1 succeeded, %2 failed, %3 not started")
                  .arg(succeededCount_)
                  .arg(failedCount_)
                  .arg(notStarted)
            : QString("Done - %1 succeeded, %2 failed").arg(succeededCount_).arg(failedCount_);
        overallLabel_->setText(summary);
        statusLabel_->setText(summary);
        return;
    }

    overallLabel_->setText(QString("Rendering flame %1 of %2").arg(currentIndex_ + 1).arg(flames_.size()));
    resultsList_->setCurrentRow(currentIndex_);

    auto renderFlame = flames_[static_cast<std::size_t>(currentIndex_)]->clone();
    renderFlame->adjustScale(widthSpin_->value(), heightSpin_->value());
    renderFlame->sampleDensity = densitySpin_->value();
    renderFlame->spatialOversample = oversampleSpin_->value();
    renderFlame->spatialFilterRadius = filterRadiusSpin_->value();
    renderFlame->enableDE = adaptiveFilterCheck_->isChecked();
    renderFlame->transparency = transparentBackgroundCheck_->isChecked();

    // B6 (FOLLOWUP_PLAN.txt) - same memory-safety clamp RenderDialog's own
    // startRender() applies, and just as relevant here: every flame in the
    // batch shares this dialog's width/height/oversample settings, so the
    // memory profile (and hence the clamp) is effectively constant across
    // the whole batch, not just this one flame.
    const ThreadBudgetResult budget = resolveMemorySafeThreadCount(*renderFlame, AppSettings::renderThreadCount());

    progress_ = std::make_unique<apo::RenderProgress>();
    progressBar_->setValue(0);
    statusLabel_->setText(budget.clamped ? QString("Using %1 thread%2 to stay under available memory...")
                                                .arg(budget.threadCount)
                                                .arg(budget.threadCount == 1 ? "" : "s")
                                          : "Rendering...");
    elapsedTimer_.start();
    progressTimer_->start();

    std::shared_ptr<const apo::Flame> shared(std::move(renderFlame));
    pendingRenderFlame_ = shared;
    const QString outputPath =
        QDir(outputFolderEdit_->text()).filePath(outputFileNames_[static_cast<std::size_t>(currentIndex_)]);
    const quint64 seed = static_cast<quint64>(std::random_device{}());
    emit fullRenderRequested(shared, seed, budget.threadCount, progress_.get(), outputPath);
}

void RenderAllDialog::onProgressTick() {
    if (!progress_) return;

    const std::uint64_t done = progress_->pointsDone.load(std::memory_order_relaxed);
    const std::uint64_t target = progress_->pointsTarget.load(std::memory_order_relaxed);
    const int percent = target > 0 ? static_cast<int>(std::min<std::uint64_t>(100, done * 100 / target)) : 0;
    progressBar_->setValue(percent);

    const double elapsedSec = elapsedTimer_.elapsed() / 1000.0;
    statusLabel_->setText(QString("%1 / %2 points (%3%) - %4s elapsed")
                               .arg(done)
                               .arg(target)
                               .arg(percent)
                               .arg(elapsedSec, 0, 'f', 1));
}

void RenderAllDialog::onFullRenderFinished(QImage /*image*/, quint64 /*pointsGenerated*/, quint64 pointsAccepted,
                                            bool cancelled, bool saved) {
    progressTimer_->stop();
    progress_.reset();

    const int index = currentIndex_;
    QString suffix;
    if (cancelled) {
        cancelledMidBatch_ = true;
        suffix = " - cancelled";
    } else if (!saved) {
        ++failedCount_;
        suffix = " - failed to save";
    } else {
        ++succeededCount_;
        suffix = QString(" - done (%1 points)").arg(pointsAccepted);

        if (saveParametersCheck_->isChecked() && pendingRenderFlame_) {
            QString flamePath = outputFileNames_[static_cast<std::size_t>(index)];
            flamePath = flamePath.left(flamePath.length() - 4) + ".flame"; // strip ".png", add ".flame"
            flamePath = QDir(outputFolderEdit_->text()).filePath(flamePath);
            if (apo::saveFlameFile(flamePath.toStdString(), {pendingRenderFlame_.get()})) {
                suffix += ", parameters saved";
            } else {
                suffix += ", failed to save parameters";
            }
        }
    }

    if (index >= 0 && index < resultsList_->count()) {
        QListWidgetItem* item = resultsList_->item(index);
        item->setText(item->text() + suffix);
    }
    pendingRenderFlame_.reset();

    // A cancel request only ever targets the render currently in flight -
    // once it's honored, stop the whole batch rather than moving on to the
    // next flame (forcing currentIndex_ past the end routes back through
    // renderNext()'s own "batch finished" branch above, rather than
    // duplicating that bookkeeping here).
    currentIndex_ = cancelled ? static_cast<int>(flames_.size()) : currentIndex_ + 1;
    renderNext();
}

void RenderAllDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

void RenderAllDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    // Same "shown, not rendered" settled state RenderDialog's own showEvent
    // screenshots at - see its own comment.
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
