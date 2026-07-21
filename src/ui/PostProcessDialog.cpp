#include "PostProcessDialog.h"

#include <algorithm>
#include <random>

#include <QColorDialog>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSplitter>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "FileDialogSupport.h"
#include "RenderWorker.h"
#include "SliderSpin.h"

namespace apo::ui {

namespace {
// Same poll cadence as RenderDialog's progress bar - see its own comment.
constexpr int kProgressPollIntervalMs = 150;
} // namespace

PostProcessDialog::PostProcessDialog(std::shared_ptr<const apo::Flame> flame, QWidget* parent)
    : QDialog(parent), flame_(flame->clone()) {
    setWindowTitle(
        QString("Post Process - %1").arg(flame_->name.empty() ? "Untitled" : QString::fromStdString(flame_->name)));
    resize(820, 480);
    setAttribute(Qt::WA_DeleteOnClose);

    initialFilterRadius_ = flame_->spatialFilterRadius;
    initialGamma_ = flame_->gamma;
    initialBrightness_ = flame_->brightness;
    initialContrast_ = flame_->contrast;
    initialVibrancy_ = flame_->vibrancy;
    initialBackground_ = flame_->background;

    auto* rootLayout = new QVBoxLayout(this);
    auto* splitter = new QSplitter(this);

    previewLabel_ = new QLabel(splitter);
    previewLabel_->setMinimumSize(200, 150);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setStyleSheet("background-color: #202020;");
    previewLabel_->setScaledContents(true);
    splitter->addWidget(previewLabel_);

    auto* controls = new QWidget(splitter);
    auto* controlsLayout = new QVBoxLayout(controls);

    // Ranges match formPostProcess.pas's own btnApplyClick/DragPanelMouseMove
    // clamp bounds exactly (Filter Radius/Gamma/Contrast/Vibrancy: 0.01..2 or
    // 0.01..10; Brightness: 0.01..100).
    filterRadiusCtrl_ = new SliderSpin("Filter Radius", 0.01, 2, flame_->spatialFilterRadius, 2, controls);
    filterRadiusCtrl_->setObjectName("filterRadiusCtrl");
    gammaCtrl_ = new SliderSpin("Gamma", 0.01, 10, flame_->gamma, 2, controls);
    gammaCtrl_->setObjectName("gammaCtrl");
    brightnessCtrl_ = new SliderSpin("Brightness", 0.01, 100, flame_->brightness, 2, controls);
    brightnessCtrl_->setObjectName("brightnessCtrl");
    contrastCtrl_ = new SliderSpin("Contrast", 0.01, 10, flame_->contrast, 2, controls);
    contrastCtrl_->setObjectName("contrastCtrl");
    vibrancyCtrl_ = new SliderSpin("Vibrancy", 0.01, 10, flame_->vibrancy, 2, controls);
    vibrancyCtrl_->setObjectName("vibrancyCtrl");
    controlsLayout->addWidget(filterRadiusCtrl_);
    controlsLayout->addWidget(gammaCtrl_);
    controlsLayout->addWidget(brightnessCtrl_);
    controlsLayout->addWidget(contrastCtrl_);
    controlsLayout->addWidget(vibrancyCtrl_);

    auto* bgRow = new QHBoxLayout();
    bgRow->addWidget(new QLabel("Background", controls));
    backgroundButton_ = new QPushButton(controls);
    backgroundButton_->setObjectName("backgroundButton");
    backgroundButton_->setFixedWidth(80);
    connect(backgroundButton_, &QPushButton::clicked, this, &PostProcessDialog::pickBackgroundColor);
    bgRow->addWidget(backgroundButton_);
    bgRow->addStretch(1);
    controlsLayout->addLayout(bgRow);
    refreshBackgroundSwatch();

    resetButton_ = new QPushButton("Reset", controls);
    resetButton_->setObjectName("resetButton");
    controlsLayout->addWidget(resetButton_);
    controlsLayout->addStretch(1);

    progressBar_ = new QProgressBar(controls);
    progressBar_->setObjectName("progressBar");
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    controlsLayout->addWidget(progressBar_);

    statusLabel_ = new QLabel("Ready", controls);
    statusLabel_->setObjectName("statusLabel");
    controlsLayout->addWidget(statusLabel_);

    saveButton_ = new QPushButton("Save...", controls);
    saveButton_->setObjectName("saveButton");
    controlsLayout->addWidget(saveButton_);

    splitter->addWidget(controls);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    rootLayout->addWidget(splitter, /*stretch=*/1);

    auto wire = [this](SliderSpin* ctrl, double apo::Flame::*field) {
        connect(ctrl, &SliderSpin::valueChanged, this, [this, field](double v) {
            flame_.get()->*field = v;
            requestPreviewRender();
        });
    };
    wire(filterRadiusCtrl_, &apo::Flame::spatialFilterRadius);
    wire(gammaCtrl_, &apo::Flame::gamma);
    wire(brightnessCtrl_, &apo::Flame::brightness);
    wire(contrastCtrl_, &apo::Flame::contrast);
    wire(vibrancyCtrl_, &apo::Flame::vibrancy);
    connect(resetButton_, &QPushButton::clicked, this, &PostProcessDialog::onReset);
    connect(saveButton_, &QPushButton::clicked, this, &PostProcessDialog::onSaveClicked);

    progressTimer_ = new QTimer(this);
    progressTimer_->setInterval(kProgressPollIntervalMs);
    connect(progressTimer_, &QTimer::timeout, this, &PostProcessDialog::onProgressTick);

    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &PostProcessDialog::renderRequested, worker_, &RenderWorker::renderFlame);
    connect(worker_, &RenderWorker::renderFinished, this, &PostProcessDialog::onRenderFinished);
    connect(this, &PostProcessDialog::fullRenderRequested, worker_, &RenderWorker::renderFull);
    connect(worker_, &RenderWorker::fullRenderFinished, this, &PostProcessDialog::onFullRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();

    requestPreviewRender();
}

PostProcessDialog::~PostProcessDialog() {
    if (progress_) progress_->cancelRequested.store(true, std::memory_order_relaxed);
    workerThread_->quit();
    workerThread_->wait();
}

void PostProcessDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    requestPreviewRender();
}

void PostProcessDialog::requestPreviewRender() {
    if (renderInFlight_) {
        renderDirty_ = true;
        return;
    }
    renderInFlight_ = true;

    auto previewFlame = flame_->clone();
    const int pw = std::max(previewLabel_->width(), 64);
    const int ph = std::max(previewLabel_->height(), 64);
    previewFlame->adjustScale(pw, ph);
    previewFlame->sampleDensity = AppSettings::previewSampleDensity();

    std::shared_ptr<const apo::Flame> shared(std::move(previewFlame));
    emit renderRequested(shared, /*seed=*/1);
}

void PostProcessDialog::onRenderFinished(QImage image, quint64 /*pointsGenerated*/, quint64 /*pointsAccepted*/) {
    if (!image.isNull()) previewLabel_->setPixmap(QPixmap::fromImage(image));

    renderInFlight_ = false;
    if (renderDirty_) {
        renderDirty_ = false;
        requestPreviewRender();
        return;
    }

    if (!autoScreenshotPath_.isEmpty()) {
        const QString path = autoScreenshotPath_;
        const bool exitAfter = autoScreenshotExit_;
        autoScreenshotPath_.clear();
        QTimer::singleShot(100, this, [this, path, exitAfter] {
            grab().save(path, "PNG");
            if (exitAfter) qApp->quit();
        });
    }
}

void PostProcessDialog::pickBackgroundColor() {
    const auto& bg = flame_->background;
    const QColor initial(bg[0], bg[1], bg[2]);
    const QColor picked = QColorDialog::getColor(initial, this, "Background Color");
    if (!picked.isValid()) return;

    flame_->background[0] = picked.red();
    flame_->background[1] = picked.green();
    flame_->background[2] = picked.blue();
    refreshBackgroundSwatch();
    requestPreviewRender();
}

void PostProcessDialog::refreshBackgroundSwatch() {
    const auto& bg = flame_->background;
    backgroundButton_->setStyleSheet(QString("background-color: rgb(%1,%2,%3);").arg(bg[0]).arg(bg[1]).arg(bg[2]));
}

void PostProcessDialog::onReset() {
    flame_->spatialFilterRadius = initialFilterRadius_;
    flame_->gamma = initialGamma_;
    flame_->brightness = initialBrightness_;
    flame_->contrast = initialContrast_;
    flame_->vibrancy = initialVibrancy_;
    flame_->background = initialBackground_;

    filterRadiusCtrl_->setValue(initialFilterRadius_);
    gammaCtrl_->setValue(initialGamma_);
    brightnessCtrl_->setValue(initialBrightness_);
    contrastCtrl_->setValue(initialContrast_);
    vibrancyCtrl_->setValue(initialVibrancy_);
    refreshBackgroundSwatch();

    requestPreviewRender();
}

void PostProcessDialog::setControlsEnabled(bool enabled) {
    filterRadiusCtrl_->setEnabled(enabled);
    gammaCtrl_->setEnabled(enabled);
    brightnessCtrl_->setEnabled(enabled);
    contrastCtrl_->setEnabled(enabled);
    vibrancyCtrl_->setEnabled(enabled);
    backgroundButton_->setEnabled(enabled);
    resetButton_->setEnabled(enabled);
    saveButton_->setEnabled(enabled);
}

void PostProcessDialog::onSaveClicked() {
    if (fullRenderInFlight_) return;

    const QString suggested = flame_->name.empty() ? "untitled.png" : QString::fromStdString(flame_->name) + ".png";
    const QString path = QFileDialog::getSaveFileName(this, "Save Image", suggested, "PNG Images (*.png)", nullptr,
                                                       testFriendlyFileDialogOptions());
    if (path.isEmpty()) return;

    progress_ = std::make_unique<apo::RenderProgress>();
    fullRenderInFlight_ = true;
    setControlsEnabled(false);
    progressBar_->setValue(0);
    statusLabel_->setText("Rendering...");
    elapsedTimer_.start();
    progressTimer_->start();

    std::shared_ptr<const apo::Flame> shared(flame_->clone());
    const quint64 seed = static_cast<quint64>(std::random_device{}());
    emit fullRenderRequested(shared, seed, AppSettings::renderThreadCount(), progress_.get(), path);
}

void PostProcessDialog::onProgressTick() {
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

void PostProcessDialog::onFullRenderFinished(QImage /*image*/, quint64 /*pointsGenerated*/, quint64 pointsAccepted,
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
        statusLabel_->setText(
            QString("Saved - %1 points accepted in %2s").arg(pointsAccepted).arg(elapsedSec, 0, 'f', 1));
    }
}

void PostProcessDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

} // namespace apo::ui
