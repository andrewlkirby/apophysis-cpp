#include "CurvesDialog.h"

#include <algorithm>

#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSplitter>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "CurvesWidget.h"
#include "RenderWorker.h"
#include "SliderSpin.h"

namespace apo::ui {

CurvesDialog::CurvesDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent)
    : QDialog(parent), flame_(std::move(flame)), initialCurves_(flame_->curves) {
    setWindowTitle(QString("Curves - %1").arg(flame_->name.empty() ? "Untitled" : QString::fromStdString(flame_->name)));
    resize(760, 520);
    setAttribute(Qt::WA_DeleteOnClose);

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

    auto* channelRow = new QHBoxLayout();
    channelRow->addWidget(new QLabel("Channel", controls));
    channelCombo_ = new QComboBox(controls);
    channelCombo_->setObjectName("channelCombo");
    // Matches Curves.pas's TCurvesChannel ordering exactly (0=All/master
    // applied first, then the per-channel curve - see Renderer.cpp's
    // toneMap()).
    channelCombo_->addItems({"All", "Red", "Green", "Blue"});
    channelRow->addWidget(channelCombo_, 1);
    controlsLayout->addLayout(channelRow);

    curvesWidget_ = new CurvesWidget(controls);
    curvesWidget_->setObjectName("curvesWidget");
    curvesWidget_->setCurves(flame_->curves);
    controlsLayout->addWidget(curvesWidget_, 1);

    // Range/decimals match the original's ControlPoint weight field:
    // stored as a raw multiplier (default 1.0), edited in the Delphi UI as
    // a 0..160 slider Position that's divided by 10 to get the weight.
    weight1Ctrl_ = new SliderSpin("Weight 1", 0, 16, 1.0, 1, controls);
    weight1Ctrl_->setObjectName("weight1Ctrl");
    weight2Ctrl_ = new SliderSpin("Weight 2", 0, 16, 1.0, 1, controls);
    weight2Ctrl_->setObjectName("weight2Ctrl");
    controlsLayout->addWidget(weight1Ctrl_);
    controlsLayout->addWidget(weight2Ctrl_);

    auto* buttonRow = new QHBoxLayout();
    resetButton_ = new QPushButton("Reset Channel", controls);
    resetButton_->setObjectName("resetButton");
    resetAllButton_ = new QPushButton("Reset All", controls);
    resetAllButton_->setObjectName("resetAllButton");
    buttonRow->addWidget(resetButton_);
    buttonRow->addWidget(resetAllButton_);
    controlsLayout->addLayout(buttonRow);

    splitter->addWidget(controls);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    rootLayout->addWidget(splitter, /*stretch=*/1);

    connect(channelCombo_, &QComboBox::currentIndexChanged, this, &CurvesDialog::onChannelChanged);
    connect(curvesWidget_, &CurvesWidget::curveChanged, this, &CurvesDialog::onCurveChanged);
    connect(weight1Ctrl_, &SliderSpin::valueChanged, this, [this](double v) { onWeightChanged(1, v); });
    connect(weight2Ctrl_, &SliderSpin::valueChanged, this, [this](double v) { onWeightChanged(2, v); });
    connect(resetButton_, &QPushButton::clicked, this, &CurvesDialog::onReset);
    connect(resetAllButton_, &QPushButton::clicked, this, &CurvesDialog::onResetAll);

    refreshWeightControls();

    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &CurvesDialog::renderRequested, worker_, &RenderWorker::renderFlame);
    connect(worker_, &RenderWorker::renderFinished, this, &CurvesDialog::onRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();

    requestPreviewRender();
}

CurvesDialog::~CurvesDialog() {
    workerThread_->quit();
    workerThread_->wait();
}

void CurvesDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    requestPreviewRender();
}

void CurvesDialog::onChannelChanged(int index) {
    if (index < 0 || index > 3) return;
    curvesWidget_->setActiveChannel(index);
    refreshWeightControls();
}

void CurvesDialog::onCurveChanged() {
    flame_->curves = curvesWidget_->curves();
    requestPreviewRender();
    emit flameChanged();
}

void CurvesDialog::onWeightChanged(int pointIndex, double value) {
    curvesWidget_->setActiveChannelWeight(pointIndex, value);
    // curvesWidget_->curveChanged() (emitted by setActiveChannelWeight)
    // already drives onCurveChanged() above, which copies the curve back
    // into flame_ and requests a fresh preview.
}

void CurvesDialog::onReset() {
    const int channel = curvesWidget_->activeChannel();
    auto curves = curvesWidget_->curves();
    curves[static_cast<size_t>(channel)] = initialCurves_[static_cast<size_t>(channel)];
    curvesWidget_->setCurves(curves);
    flame_->curves = curves;
    refreshWeightControls();
    requestPreviewRender();
    emit flameChanged();
}

void CurvesDialog::onResetAll() {
    curvesWidget_->setCurves(initialCurves_);
    flame_->curves = initialCurves_;
    refreshWeightControls();
    requestPreviewRender();
    emit flameChanged();
}

void CurvesDialog::refreshWeightControls() {
    const auto& curve = curvesWidget_->curves()[static_cast<size_t>(curvesWidget_->activeChannel())];
    weight1Ctrl_->setValue(curve.weights[1]);
    weight2Ctrl_->setValue(curve.weights[2]);
}

void CurvesDialog::requestPreviewRender() {
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

void CurvesDialog::onRenderFinished(QImage image, quint64 /*pointsGenerated*/, quint64 /*pointsAccepted*/) {
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

void CurvesDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

} // namespace apo::ui
