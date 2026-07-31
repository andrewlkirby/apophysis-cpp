#include "AdjustPanel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <random>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "GradientBrowserDialog.h"
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

AdjustPanel::AdjustPanel(std::shared_ptr<apo::Flame> flame, QWidget* parent)
    : QWidget(parent), flame_(std::move(flame)), initialCmap_(flame_->cmap), gradientBaseline_(flame_->cmap) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildCameraTab(), "Camera");
    tabs->addTab(buildColoringTab(), "Coloring");
    tabs->addTab(buildGradientTab(), "Gradient");
    tabs->addTab(buildSizeTab(), "Size");
    rootLayout->addWidget(tabs);

    refreshControlsFromFlame();
}

void AdjustPanel::refreshControlsFromFlame() {
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

    gradientBaseline_ = flame_->cmap;
    refreshGradientStrip();
}

QWidget* AdjustPanel::buildCameraTab() {
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
            emit propertyEdited();
        });
        connect(ctrl, &SliderSpin::editingFinished, this, [this](double) { commitEditIfNeeded(); });
    };
    auto wirePlain = [this](SliderSpin* ctrl, double apo::Flame::*field) {
        connect(ctrl, &SliderSpin::valueChanged, this, [this, field](double v) {
            beginEditIfNeeded();
            flame_.get()->*field = v;
            emit propertyEdited();
        });
        connect(ctrl, &SliderSpin::editingFinished, this, [this](double) { commitEditIfNeeded(); });
    };

    wirePlain(zoomCtrl_, &apo::Flame::zoom);
    connect(centerXCtrl_, &SliderSpin::valueChanged, this, [this](double v) {
        beginEditIfNeeded();
        flame_->center[0] = v;
        emit propertyEdited();
    });
    connect(centerXCtrl_, &SliderSpin::editingFinished, this, [this](double) { commitEditIfNeeded(); });
    connect(centerYCtrl_, &SliderSpin::valueChanged, this, [this](double v) {
        beginEditIfNeeded();
        flame_->center[1] = v;
        emit propertyEdited();
    });
    connect(centerYCtrl_, &SliderSpin::editingFinished, this, [this](double) { commitEditIfNeeded(); });
    wireDegrees(angleCtrl_, &apo::Flame::angle);
    wireDegrees(pitchCtrl_, &apo::Flame::cameraPitch);
    wireDegrees(yawCtrl_, &apo::Flame::cameraYaw);
    wirePlain(perspCtrl_, &apo::Flame::cameraPersp);
    wirePlain(dofCtrl_, &apo::Flame::cameraDOF);
    wirePlain(zposCtrl_, &apo::Flame::cameraZpos);

    return tab;
}

QWidget* AdjustPanel::buildColoringTab() {
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
    connect(backgroundButton_, &QPushButton::clicked, this, &AdjustPanel::pickBackgroundColor);
    bgRow->addWidget(backgroundButton_);
    bgRow->addStretch(1);
    layout->addLayout(bgRow);
    layout->addStretch(1);

    auto wire = [this](SliderSpin* ctrl, double apo::Flame::*field) {
        connect(ctrl, &SliderSpin::valueChanged, this, [this, field](double v) {
            beginEditIfNeeded();
            flame_.get()->*field = v;
            emit propertyEdited();
        });
        connect(ctrl, &SliderSpin::editingFinished, this, [this](double) { commitEditIfNeeded(); });
    };
    wire(gammaCtrl_, &apo::Flame::gamma);
    wire(brightnessCtrl_, &apo::Flame::brightness);
    wire(vibrancyCtrl_, &apo::Flame::vibrancy);
    wire(gammaThresholdCtrl_, &apo::Flame::gammaThreshold);

    return tab;
}

QWidget* AdjustPanel::buildGradientTab() {
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

    connect(gradientModeCombo_, &QComboBox::currentIndexChanged, this, &AdjustPanel::setGradientMode);
    connect(gradientAmountCtrl_, &SliderSpin::valueChanged, this, [this](double v) {
        beginEditIfNeeded();
        applyGradientAmount(v);
    });
    connect(gradientAmountCtrl_, &SliderSpin::editingFinished, this, [this](double v) {
        applyGradientAmount(v);
        gradientBaseline_ = flame_->cmap;
        commitEditIfNeeded();
    });

    connect(invertButton, &QPushButton::clicked, this, [this] {
        beginEditIfNeeded();
        flame_->cmap = apo::invertGradient(flame_->cmap);
        gradientBaseline_ = flame_->cmap;
        refreshGradientStrip();
        emit propertyEdited();
        commitEditIfNeeded();
    });
    connect(reverseButton, &QPushButton::clicked, this, [this] {
        beginEditIfNeeded();
        flame_->cmap = apo::reverseGradient(flame_->cmap);
        gradientBaseline_ = flame_->cmap;
        refreshGradientStrip();
        emit propertyEdited();
        commitEditIfNeeded();
    });
    connect(randomizeButton, &QPushButton::clicked, this, [this] {
        beginEditIfNeeded();
        apo::Xoshiro256Rng rng(std::random_device{}());
        flame_->cmap = apo::randomGradient(rng);
        gradientBaseline_ = flame_->cmap;
        refreshGradientStrip();
        emit propertyEdited();
        commitEditIfNeeded();
    });
    connect(resetButton, &QPushButton::clicked, this, [this] {
        beginEditIfNeeded();
        flame_->cmap = initialCmap_;
        gradientBaseline_ = flame_->cmap;
        refreshGradientStrip();
        emit propertyEdited();
        commitEditIfNeeded();
    });
    connect(browseButton, &QPushButton::clicked, this, &AdjustPanel::openGradientBrowser);

    return tab;
}

void AdjustPanel::openGradientBrowser() {
    auto* browser = new GradientBrowserDialog(this);
    connect(browser, &GradientBrowserDialog::gradientApplied, this, [this](apo::ColorMap cmap) {
        beginEditIfNeeded();
        flame_->cmap = cmap;
        gradientBaseline_ = flame_->cmap;
        refreshGradientStrip();
        emit propertyEdited();
        commitEditIfNeeded();
    });
    browser->show();
}

bool AdjustPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == gradientStrip_ && event->type() == QEvent::MouseButtonRelease) {
        if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) openGradientBrowser();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

QWidget* AdjustPanel::buildSizeTab() {
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
        emit propertyEdited();
        commitEditIfNeeded();
    });

    return tab;
}

void AdjustPanel::refreshGradientStrip() {
    gradientStrip_->setPixmap(QPixmap::fromImage(colorMapToImage(flame_->cmap)));
}

void AdjustPanel::refreshBackgroundSwatch() {
    const auto& bg = flame_->background;
    backgroundButton_->setStyleSheet(QString("background-color: rgb(%1,%2,%3);").arg(bg[0]).arg(bg[1]).arg(bg[2]));
}

void AdjustPanel::pickBackgroundColor() {
    const auto& bg = flame_->background;
    const QColor initial(bg[0], bg[1], bg[2]);
    const QColor picked = QColorDialog::getColor(initial, this, "Background Color");
    if (!picked.isValid()) return;

    beginEditIfNeeded();
    flame_->background[0] = picked.red();
    flame_->background[1] = picked.green();
    flame_->background[2] = picked.blue();
    refreshBackgroundSwatch();
    emit propertyEdited();
    commitEditIfNeeded();
}

void AdjustPanel::setGradientMode(int modeIndex) {
    if (modeIndex < 0 || modeIndex >= static_cast<int>(std::size(kGradientModes))) return;
    gradientBaseline_ = flame_->cmap;
    const auto& mode = kGradientModes[modeIndex];
    gradientAmountCtrl_->setRange(mode.min, mode.max, mode.defaultValue);
}

void AdjustPanel::applyGradientAmount(double amount) {
    const int modeIndex = gradientModeCombo_->currentIndex();
    if (modeIndex < 0) return;

    switch (modeIndex) {
        case 0: flame_->cmap = apo::rotateGradient(gradientBaseline_, static_cast<int>(std::lround(amount))); break;
        case 1: flame_->cmap = apo::hueShiftGradient(gradientBaseline_, amount); break;
        case 2: flame_->cmap = apo::saturationShiftGradient(gradientBaseline_, amount); break;
        case 3:
            flame_->cmap = apo::brightnessShiftGradient(gradientBaseline_, static_cast<int>(std::lround(amount)));
            break;
        case 4:
            flame_->cmap = apo::contrastShiftGradient(gradientBaseline_, static_cast<int>(std::lround(amount)));
            break;
        case 5: flame_->cmap = apo::blurGradient(gradientBaseline_, static_cast<int>(std::lround(amount))); break;
        case 6: flame_->cmap = apo::frequencyGradient(gradientBaseline_, static_cast<int>(std::lround(amount))); break;
        default: break;
    }
    refreshGradientStrip();
    emit propertyEdited();
}

void AdjustPanel::beginEditIfNeeded() {
    if (hasPendingGesture_) return;
    hasPendingGesture_ = true;
    emit editingStarted();
}

void AdjustPanel::commitEditIfNeeded() {
    if (!hasPendingGesture_) return;
    hasPendingGesture_ = false;
    emit editingFinished();
}

} // namespace apo::ui
