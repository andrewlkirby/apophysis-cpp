#include "AdjustDialog.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <random>

#include <QAction>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "GradientBrowserDialog.h"
#include "RenderWorker.h"
#include "SliderSpin.h"
#include "core/Rng.h"
#include "core/edit/GradientOps.h"

namespace apo::ui {

namespace {

// Not <cmath>'s M_PI: MSVC only defines that behind _USE_MATH_DEFINES.
constexpr double kPi = 3.14159265358979323846;

struct GradientModeSpec {
    const char* name;
    double min, max, defaultValue;
    int decimals;
};

// Matches Adjust.pas's mnuRotate/mnuHue/.../mnuFrequency click handlers'
// ScrollBar.Min/Max/Position setup, in the same order the original's
// scrollModePopup menu listed them.
constexpr GradientModeSpec kGradientModes[] = {
    {"Rotate", -128, 128, 0, 0},
    {"Hue", -180, 180, 0, 1},
    {"Saturation", -100, 100, 0, 1},
    {"Brightness", -255, 255, 0, 0},
    {"Contrast", -100, 100, 0, 1},
    {"Blur", 0, 127, 0, 0},
    {"Frequency", 1, 10, 1, 0},
};

QImage colorMapToImage(const apo::ColorMap& cmap) {
    QImage image(256, 1, QImage::Format_RGB888);
    for (int i = 0; i < 256; ++i) {
        image.setPixelColor(i, 0, QColor(cmap.entries[i][0], cmap.entries[i][1], cmap.entries[i][2]));
    }
    return image;
}

} // namespace

bool AdjustDialog::FlameSnapshot::operator==(const FlameSnapshot& other) const {
    return zoom == other.zoom && centerX == other.centerX && centerY == other.centerY && angle == other.angle &&
           pitch == other.pitch && yaw == other.yaw && persp == other.persp && dof == other.dof &&
           zpos == other.zpos && gamma == other.gamma && brightness == other.brightness &&
           vibrancy == other.vibrancy && gammaThreshold == other.gammaThreshold && background == other.background &&
           cmap.entries == other.cmap.entries && width == other.width && height == other.height &&
           pixelsPerUnit == other.pixelsPerUnit;
}

AdjustDialog::AdjustDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent)
    : QDialog(parent), flame_(std::move(flame)), initialCmap_(flame_->cmap), gradientBaseline_(flame_->cmap) {
    setWindowTitle(QString("Adjust - %1").arg(flame_->name.empty() ? "Untitled" : QString::fromStdString(flame_->name)));
    resize(900, 600);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* rootLayout = new QVBoxLayout(this);

    auto* toolbar = new QToolBar(this);
    undoAction_ = toolbar->addAction("Undo");
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, &AdjustDialog::onUndo);
    redoAction_ = toolbar->addAction("Redo");
    redoAction_->setShortcut(QKeySequence::Redo);
    connect(redoAction_, &QAction::triggered, this, &AdjustDialog::onRedo);
    rootLayout->addWidget(toolbar);
    updateUndoRedoActions();

    auto* splitter = new QSplitter(this);

    previewLabel_ = new QLabel(splitter);
    previewLabel_->setMinimumSize(200, 150);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setStyleSheet("background-color: #202020;");
    // The very first preview render is requested before the splitter has
    // assigned this label its final layout size, so that render comes back
    // smaller than the label's eventual displayed size - scaledContents
    // stretches it to fill regardless, and resizeEvent() below requests a
    // freshly-sized render once real geometry is known (and on every
    // subsequent resize) so it isn't stuck upscaled/blurry.
    previewLabel_->setScaledContents(true);
    splitter->addWidget(previewLabel_);

    auto* tabs = new QTabWidget(splitter);
    tabs->addTab(buildCameraTab(), "Camera");
    tabs->addTab(buildColoringTab(), "Coloring");
    tabs->addTab(buildGradientTab(), "Gradient");
    tabs->addTab(buildSizeTab(), "Size");
    splitter->addWidget(tabs);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    rootLayout->addWidget(splitter, /*stretch=*/1);

    refreshControlsFromFlame();

    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &AdjustDialog::renderRequested, worker_, &RenderWorker::renderFlame);
    connect(worker_, &RenderWorker::renderFinished, this, &AdjustDialog::onRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();

    requestPreviewRender();
}

AdjustDialog::~AdjustDialog() {
    workerThread_->quit();
    workerThread_->wait();
}

void AdjustDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    requestPreviewRender();
}

QWidget* AdjustDialog::buildCameraTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    zoomCtrl_ = new SliderSpin("Zoom", -3, 3, flame_->zoom, 3, tab);
    zoomCtrl_->setObjectName("zoomCtrl");
    centerXCtrl_ = new SliderSpin("Center X", -10, 10, flame_->center[0], 3, tab);
    centerXCtrl_->setObjectName("centerXCtrl");
    centerYCtrl_ = new SliderSpin("Center Y", -10, 10, flame_->center[1], 3, tab);
    centerYCtrl_->setObjectName("centerYCtrl");
    angleCtrl_ = new SliderSpin("Angle", -360, 360, flame_->angle * 180.0 / kPi, 2, tab);
    angleCtrl_->setObjectName("angleCtrl");
    layout->addWidget(zoomCtrl_);
    layout->addWidget(centerXCtrl_);
    layout->addWidget(centerYCtrl_);
    layout->addWidget(angleCtrl_);

    auto* cameraGroup = new QGroupBox("3D Camera", tab);
    auto* cameraLayout = new QVBoxLayout(cameraGroup);
    pitchCtrl_ = new SliderSpin("Pitch", -180, 180, flame_->cameraPitch * 180.0 / kPi, 2, cameraGroup);
    pitchCtrl_->setObjectName("pitchCtrl");
    yawCtrl_ = new SliderSpin("Yaw", -180, 180, flame_->cameraYaw * 180.0 / kPi, 2, cameraGroup);
    yawCtrl_->setObjectName("yawCtrl");
    perspCtrl_ = new SliderSpin("Perspective", -2, 2, flame_->cameraPersp, 3, cameraGroup);
    perspCtrl_->setObjectName("perspCtrl");
    dofCtrl_ = new SliderSpin("Depth of Field", 0, 50, flame_->cameraDOF, 3, cameraGroup);
    dofCtrl_->setObjectName("dofCtrl");
    zposCtrl_ = new SliderSpin("Z Position", -200, 200, flame_->cameraZpos, 3, cameraGroup);
    zposCtrl_->setObjectName("zposCtrl");
    cameraLayout->addWidget(pitchCtrl_);
    cameraLayout->addWidget(yawCtrl_);
    cameraLayout->addWidget(perspCtrl_);
    cameraLayout->addWidget(dofCtrl_);
    cameraLayout->addWidget(zposCtrl_);
    layout->addWidget(cameraGroup);
    layout->addStretch(1);

    auto wireDegrees = [this](SliderSpin* ctrl, double apo::Flame::*field) {
        connect(ctrl, &SliderSpin::valueChanged, this, [this, field](double v) {
            beginEditIfNeeded();
            flame_.get()->*field = v * kPi / 180.0;
            requestPreviewRender();
        });
        connect(ctrl, &SliderSpin::editingFinished, this, [this](double) { commitEdit(); });
    };
    auto wirePlain = [this](SliderSpin* ctrl, double apo::Flame::*field) {
        connect(ctrl, &SliderSpin::valueChanged, this, [this, field](double v) {
            beginEditIfNeeded();
            flame_.get()->*field = v;
            requestPreviewRender();
        });
        connect(ctrl, &SliderSpin::editingFinished, this, [this](double) { commitEdit(); });
    };

    wirePlain(zoomCtrl_, &apo::Flame::zoom);
    connect(centerXCtrl_, &SliderSpin::valueChanged, this, [this](double v) {
        beginEditIfNeeded();
        flame_->center[0] = v;
        requestPreviewRender();
    });
    connect(centerXCtrl_, &SliderSpin::editingFinished, this, [this](double) { commitEdit(); });
    connect(centerYCtrl_, &SliderSpin::valueChanged, this, [this](double v) {
        beginEditIfNeeded();
        flame_->center[1] = v;
        requestPreviewRender();
    });
    connect(centerYCtrl_, &SliderSpin::editingFinished, this, [this](double) { commitEdit(); });
    wireDegrees(angleCtrl_, &apo::Flame::angle);
    wireDegrees(pitchCtrl_, &apo::Flame::cameraPitch);
    wireDegrees(yawCtrl_, &apo::Flame::cameraYaw);
    wirePlain(perspCtrl_, &apo::Flame::cameraPersp);
    wirePlain(dofCtrl_, &apo::Flame::cameraDOF);
    wirePlain(zposCtrl_, &apo::Flame::cameraZpos);

    return tab;
}

QWidget* AdjustDialog::buildColoringTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    gammaCtrl_ = new SliderSpin("Gamma", 0, 20, flame_->gamma, 3, tab);
    gammaCtrl_->setObjectName("gammaCtrl");
    brightnessCtrl_ = new SliderSpin("Brightness", 0, 100, flame_->brightness, 3, tab);
    brightnessCtrl_->setObjectName("brightnessCtrl");
    vibrancyCtrl_ = new SliderSpin("Vibrancy", 0, 50, flame_->vibrancy, 3, tab);
    vibrancyCtrl_->setObjectName("vibrancyCtrl");
    gammaThresholdCtrl_ = new SliderSpin("Gamma Threshold", 0, 1, flame_->gammaThreshold, 4, tab);
    gammaThresholdCtrl_->setObjectName("gammaThresholdCtrl");
    layout->addWidget(gammaCtrl_);
    layout->addWidget(brightnessCtrl_);
    layout->addWidget(vibrancyCtrl_);
    layout->addWidget(gammaThresholdCtrl_);

    auto* bgRow = new QHBoxLayout();
    bgRow->addWidget(new QLabel("Background", tab));
    backgroundButton_ = new QPushButton(tab);
    backgroundButton_->setObjectName("backgroundButton");
    backgroundButton_->setFixedWidth(80);
    connect(backgroundButton_, &QPushButton::clicked, this, &AdjustDialog::pickBackgroundColor);
    bgRow->addWidget(backgroundButton_);
    bgRow->addStretch(1);
    layout->addLayout(bgRow);
    layout->addStretch(1);

    auto wire = [this](SliderSpin* ctrl, double apo::Flame::*field) {
        connect(ctrl, &SliderSpin::valueChanged, this, [this, field](double v) {
            beginEditIfNeeded();
            flame_.get()->*field = v;
            requestPreviewRender();
        });
        connect(ctrl, &SliderSpin::editingFinished, this, [this](double) { commitEdit(); });
    };
    wire(gammaCtrl_, &apo::Flame::gamma);
    wire(brightnessCtrl_, &apo::Flame::brightness);
    wire(vibrancyCtrl_, &apo::Flame::vibrancy);
    wire(gammaThresholdCtrl_, &apo::Flame::gammaThreshold);

    return tab;
}

QWidget* AdjustDialog::buildGradientTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    gradientStrip_ = new QLabel(tab);
    gradientStrip_->setObjectName("gradientStrip");
    gradientStrip_->setMinimumHeight(40);
    gradientStrip_->setScaledContents(true);
    gradientStrip_->setCursor(Qt::PointingHandCursor);
    gradientStrip_->setToolTip("Click to browse gradients...");
    gradientStrip_->installEventFilter(this);
    layout->addWidget(gradientStrip_);

    auto* modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel("Mode", tab));
    gradientModeCombo_ = new QComboBox(tab);
    gradientModeCombo_->setObjectName("gradientModeCombo");
    for (const auto& mode : kGradientModes) gradientModeCombo_->addItem(mode.name);
    modeRow->addWidget(gradientModeCombo_, 1);
    layout->addLayout(modeRow);

    gradientAmountCtrl_ =
        new SliderSpin("Amount", kGradientModes[0].min, kGradientModes[0].max, kGradientModes[0].defaultValue,
                        kGradientModes[0].decimals, tab);
    gradientAmountCtrl_->setObjectName("gradientAmountCtrl");
    layout->addWidget(gradientAmountCtrl_);

    auto* buttonRow = new QHBoxLayout();
    auto* invertButton = new QPushButton("Invert", tab);
    invertButton->setObjectName("invertButton");
    auto* reverseButton = new QPushButton("Reverse", tab);
    reverseButton->setObjectName("reverseButton");
    auto* randomizeButton = new QPushButton("Randomize", tab);
    randomizeButton->setObjectName("randomizeButton");
    auto* resetButton = new QPushButton("Reset", tab);
    resetButton->setObjectName("resetButton");
    auto* browseButton = new QPushButton("Browse Gradients...", tab);
    browseButton->setObjectName("gradientBrowseButton");
    buttonRow->addWidget(invertButton);
    buttonRow->addWidget(reverseButton);
    buttonRow->addWidget(randomizeButton);
    buttonRow->addWidget(resetButton);
    buttonRow->addWidget(browseButton);
    layout->addLayout(buttonRow);
    layout->addStretch(1);

    connect(gradientModeCombo_, &QComboBox::currentIndexChanged, this, &AdjustDialog::setGradientMode);
    connect(gradientAmountCtrl_, &SliderSpin::valueChanged, this, [this](double v) {
        beginEditIfNeeded();
        applyGradientAmount(v);
    });
    connect(gradientAmountCtrl_, &SliderSpin::editingFinished, this, [this](double v) {
        applyGradientAmount(v);
        gradientBaseline_ = flame_->cmap;
        commitEdit();
    });

    connect(invertButton, &QPushButton::clicked, this, [this] {
        beginEditIfNeeded();
        flame_->cmap = apo::invertGradient(flame_->cmap);
        gradientBaseline_ = flame_->cmap;
        refreshGradientStrip();
        requestPreviewRender();
        commitEdit();
    });
    connect(reverseButton, &QPushButton::clicked, this, [this] {
        beginEditIfNeeded();
        flame_->cmap = apo::reverseGradient(flame_->cmap);
        gradientBaseline_ = flame_->cmap;
        refreshGradientStrip();
        requestPreviewRender();
        commitEdit();
    });
    connect(randomizeButton, &QPushButton::clicked, this, [this] {
        beginEditIfNeeded();
        apo::Xoshiro256Rng rng(std::random_device{}());
        flame_->cmap = apo::randomGradient(rng);
        gradientBaseline_ = flame_->cmap;
        refreshGradientStrip();
        requestPreviewRender();
        commitEdit();
    });
    connect(resetButton, &QPushButton::clicked, this, [this] {
        beginEditIfNeeded();
        flame_->cmap = initialCmap_;
        gradientBaseline_ = flame_->cmap;
        refreshGradientStrip();
        requestPreviewRender();
        commitEdit();
    });
    connect(browseButton, &QPushButton::clicked, this, &AdjustDialog::openGradientBrowser);

    return tab;
}

void AdjustDialog::openGradientBrowser() {
    auto* browser = new GradientBrowserDialog(this);
    connect(browser, &GradientBrowserDialog::gradientApplied, this, [this](apo::ColorMap cmap) {
        beginEditIfNeeded();
        flame_->cmap = cmap;
        gradientBaseline_ = flame_->cmap;
        refreshGradientStrip();
        requestPreviewRender();
        commitEdit();
    });
    browser->show();
}

bool AdjustDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == gradientStrip_ && event->type() == QEvent::MouseButtonRelease) {
        if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) openGradientBrowser();
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

QWidget* AdjustDialog::buildSizeTab() {
    auto* tab = new QWidget(this);
    auto* form = new QFormLayout(tab);

    widthSpin_ = new QSpinBox(tab);
    widthSpin_->setObjectName("widthSpin");
    widthSpin_->setRange(1, 16384);
    widthSpin_->setValue(flame_->width);
    heightSpin_ = new QSpinBox(tab);
    heightSpin_->setObjectName("heightSpin");
    heightSpin_->setRange(1, 16384);
    heightSpin_->setValue(flame_->height);
    maintainAspectCheck_ = new QCheckBox("Maintain aspect ratio", tab);
    sizeAspectRatio_ = static_cast<double>(flame_->width) / std::max(1, flame_->height);

    form->addRow("Width", widthSpin_);
    form->addRow("Height", heightSpin_);
    form->addRow(maintainAspectCheck_);

    auto* applyButton = new QPushButton("Apply", tab);
    applyButton->setObjectName("sizeApplyButton");
    form->addRow(applyButton);

    connect(widthSpin_, &QSpinBox::valueChanged, this, [this](int w) {
        if (maintainAspectCheck_->isChecked() && widthSpin_->hasFocus()) {
            heightSpin_->blockSignals(true);
            heightSpin_->setValue(static_cast<int>(std::lround(w / sizeAspectRatio_)));
            heightSpin_->blockSignals(false);
        }
    });
    connect(heightSpin_, &QSpinBox::valueChanged, this, [this](int h) {
        if (maintainAspectCheck_->isChecked() && heightSpin_->hasFocus()) {
            widthSpin_->blockSignals(true);
            widthSpin_->setValue(static_cast<int>(std::lround(h * sizeAspectRatio_)));
            widthSpin_->blockSignals(false);
        }
    });
    connect(applyButton, &QPushButton::clicked, this, [this] {
        beginEditIfNeeded();
        flame_->adjustScale(widthSpin_->value(), heightSpin_->value());
        sizeAspectRatio_ = static_cast<double>(flame_->width) / std::max(1, flame_->height);
        requestPreviewRender();
        commitEdit();
    });

    return tab;
}

AdjustDialog::FlameSnapshot AdjustDialog::snapshot() const {
    FlameSnapshot s;
    s.zoom = flame_->zoom;
    s.centerX = flame_->center[0];
    s.centerY = flame_->center[1];
    s.angle = flame_->angle;
    s.pitch = flame_->cameraPitch;
    s.yaw = flame_->cameraYaw;
    s.persp = flame_->cameraPersp;
    s.dof = flame_->cameraDOF;
    s.zpos = flame_->cameraZpos;
    s.gamma = flame_->gamma;
    s.brightness = flame_->brightness;
    s.vibrancy = flame_->vibrancy;
    s.gammaThreshold = flame_->gammaThreshold;
    s.background = flame_->background;
    s.cmap = flame_->cmap;
    s.width = flame_->width;
    s.height = flame_->height;
    s.pixelsPerUnit = flame_->pixelsPerUnit;
    return s;
}

void AdjustDialog::applySnapshot(const FlameSnapshot& s) {
    flame_->zoom = s.zoom;
    flame_->center[0] = s.centerX;
    flame_->center[1] = s.centerY;
    flame_->angle = s.angle;
    flame_->cameraPitch = s.pitch;
    flame_->cameraYaw = s.yaw;
    flame_->cameraPersp = s.persp;
    flame_->cameraDOF = s.dof;
    flame_->cameraZpos = s.zpos;
    flame_->gamma = s.gamma;
    flame_->brightness = s.brightness;
    flame_->vibrancy = s.vibrancy;
    flame_->gammaThreshold = s.gammaThreshold;
    flame_->background = s.background;
    flame_->cmap = s.cmap;
    flame_->width = s.width;
    flame_->height = s.height;
    flame_->pixelsPerUnit = s.pixelsPerUnit;
    gradientBaseline_ = flame_->cmap;
}

void AdjustDialog::refreshControlsFromFlame() {
    zoomCtrl_->setValue(flame_->zoom);
    centerXCtrl_->setValue(flame_->center[0]);
    centerYCtrl_->setValue(flame_->center[1]);
    angleCtrl_->setValue(flame_->angle * 180.0 / kPi);
    pitchCtrl_->setValue(flame_->cameraPitch * 180.0 / kPi);
    yawCtrl_->setValue(flame_->cameraYaw * 180.0 / kPi);
    perspCtrl_->setValue(flame_->cameraPersp);
    dofCtrl_->setValue(flame_->cameraDOF);
    zposCtrl_->setValue(flame_->cameraZpos);

    gammaCtrl_->setValue(flame_->gamma);
    brightnessCtrl_->setValue(flame_->brightness);
    vibrancyCtrl_->setValue(flame_->vibrancy);
    gammaThresholdCtrl_->setValue(flame_->gammaThreshold);
    refreshBackgroundSwatch();

    widthSpin_->blockSignals(true);
    heightSpin_->blockSignals(true);
    widthSpin_->setValue(flame_->width);
    heightSpin_->setValue(flame_->height);
    widthSpin_->blockSignals(false);
    heightSpin_->blockSignals(false);
    sizeAspectRatio_ = static_cast<double>(flame_->width) / std::max(1, flame_->height);

    refreshGradientStrip();
}

void AdjustDialog::refreshGradientStrip() {
    gradientStrip_->setPixmap(QPixmap::fromImage(colorMapToImage(flame_->cmap)));
}

void AdjustDialog::refreshBackgroundSwatch() {
    const auto& bg = flame_->background;
    backgroundButton_->setStyleSheet(
        QString("background-color: rgb(%1,%2,%3);").arg(bg[0]).arg(bg[1]).arg(bg[2]));
}

void AdjustDialog::beginEditIfNeeded() {
    if (hasPendingSnapshot_) return;
    pendingBefore_ = snapshot();
    hasPendingSnapshot_ = true;
}

void AdjustDialog::commitEdit() {
    if (!hasPendingSnapshot_) return;
    hasPendingSnapshot_ = false;
    const FlameSnapshot after = snapshot();
    if (pendingBefore_ == after) return;

    undoStack_.push_back({pendingBefore_, after});
    redoStack_.clear();
    updateUndoRedoActions();
    emit flameChanged();
}

void AdjustDialog::updateUndoRedoActions() {
    undoAction_->setEnabled(!undoStack_.empty());
    redoAction_->setEnabled(!redoStack_.empty());
}

void AdjustDialog::onUndo() {
    if (undoStack_.empty()) return;
    const UndoEntry entry = undoStack_.back();
    undoStack_.pop_back();
    applySnapshot(entry.before);
    redoStack_.push_back(entry);
    refreshControlsFromFlame();
    updateUndoRedoActions();
    requestPreviewRender();
    emit flameChanged();
}

void AdjustDialog::onRedo() {
    if (redoStack_.empty()) return;
    const UndoEntry entry = redoStack_.back();
    redoStack_.pop_back();
    applySnapshot(entry.after);
    undoStack_.push_back(entry);
    refreshControlsFromFlame();
    updateUndoRedoActions();
    requestPreviewRender();
    emit flameChanged();
}

void AdjustDialog::requestPreviewRender() {
    if (renderInFlight_) {
        renderDirty_ = true;
        return;
    }
    renderInFlight_ = true;

    auto previewFlame = flame_->clone();
    const int pw = std::max(previewLabel_->width(), 64);
    const int ph = std::max(previewLabel_->height(), 64);
    previewFlame->adjustScale(pw, ph);
    // Read fresh on every preview render (not cached) so a Preview Quality
    // change in OptionsDialog takes effect immediately on the very next
    // render, without needing this dialog to be reopened.
    previewFlame->sampleDensity = AppSettings::previewSampleDensity();

    std::shared_ptr<const apo::Flame> shared(std::move(previewFlame));
    emit renderRequested(shared, /*seed=*/1);
}

void AdjustDialog::onRenderFinished(QImage image, quint64 /*pointsGenerated*/, quint64 /*pointsAccepted*/) {
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

void AdjustDialog::pickBackgroundColor() {
    const auto& bg = flame_->background;
    const QColor initial(bg[0], bg[1], bg[2]);
    const QColor picked = QColorDialog::getColor(initial, this, "Background Color");
    if (!picked.isValid()) return;

    beginEditIfNeeded();
    flame_->background[0] = picked.red();
    flame_->background[1] = picked.green();
    flame_->background[2] = picked.blue();
    refreshBackgroundSwatch();
    requestPreviewRender();
    commitEdit();
}

void AdjustDialog::setGradientMode(int modeIndex) {
    if (modeIndex < 0 || modeIndex >= static_cast<int>(std::size(kGradientModes))) return;
    gradientBaseline_ = flame_->cmap;
    const auto& mode = kGradientModes[modeIndex];
    gradientAmountCtrl_->setRange(mode.min, mode.max, mode.defaultValue);
}

void AdjustDialog::applyGradientAmount(double amount) {
    const int modeIndex = gradientModeCombo_->currentIndex();
    if (modeIndex < 0) return;

    switch (modeIndex) {
        case 0: flame_->cmap = apo::rotateGradient(gradientBaseline_, static_cast<int>(std::lround(amount))); break;
        case 1: flame_->cmap = apo::hueShiftGradient(gradientBaseline_, amount); break;
        case 2: flame_->cmap = apo::saturationShiftGradient(gradientBaseline_, amount); break;
        case 3: flame_->cmap = apo::brightnessShiftGradient(gradientBaseline_, static_cast<int>(std::lround(amount))); break;
        case 4: flame_->cmap = apo::contrastShiftGradient(gradientBaseline_, static_cast<int>(std::lround(amount))); break;
        case 5: flame_->cmap = apo::blurGradient(gradientBaseline_, static_cast<int>(std::lround(amount))); break;
        case 6: flame_->cmap = apo::frequencyGradient(gradientBaseline_, static_cast<int>(std::lround(amount))); break;
        default: break;
    }
    refreshGradientStrip();
    requestPreviewRender();
}

void AdjustDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

} // namespace apo::ui
