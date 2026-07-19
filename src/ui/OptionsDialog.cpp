#include "OptionsDialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "core/VariationRegistry.h"

namespace apo::ui {

namespace {
// Mirrors Options.pas's High/Medium/Low preview-quality presets - see
// AppSettings.h's doc comment on why these are fixed values rather than
// user-editable numbers.
struct PreviewQualityPreset {
    const char* name;
    double density;
};
constexpr PreviewQualityPreset kPreviewQualityPresets[] = {
    {"Low", 4.0},
    {"Medium", 8.0},
    {"High", 16.0},
};

// Matches Options.dfm's cmbSymType.Items.Strings exactly, and
// core/edit/Symmetry.h's addSymmetry() sign convention (see
// AppSettings.h's doc comment on randomSymmetryType/Order).
constexpr const char* kSymmetryTypeNames[] = {"None", "Bilateral", "Rotational", "Dihedral"};
} // namespace

OptionsDialog::OptionsDialog(QWidget* parent) : QDialog(parent) {
    // Unlike Adjust/Render (non-modal, opened via show(), so they need
    // WA_DeleteOnClose to self-clean when the user closes the window),
    // this dialog is meant to be used modally via exec() - no
    // WA_DeleteOnClose, since combining it with a stack-allocated (or
    // otherwise externally-owned) instance would double-delete once its
    // deferred deleteLater() fires after the caller's own object already
    // went out of scope.
    setWindowTitle("Options");
    resize(420, 420);

    auto* rootLayout = new QVBoxLayout(this);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildGeneralTab(), "General");
    tabs->addTab(buildRandomTab(), "Random");
    tabs->addTab(buildVariationsTab(), "Variations");
    tabs->addTab(buildNewFlameTab(), "New Flame");
    rootLayout->addWidget(tabs, /*stretch=*/1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &OptionsDialog::applyAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);
}

QWidget* OptionsDialog::buildGeneralTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    auto* perfGroup = new QGroupBox("Performance", tab);
    auto* perfForm = new QFormLayout(perfGroup);
    threadCountSpin_ = new QSpinBox(perfGroup);
    threadCountSpin_->setObjectName("threadCountSpin");
    const int maxThreads = std::max(1, QThread::idealThreadCount());
    threadCountSpin_->setRange(0, maxThreads);
    threadCountSpin_->setSpecialValueText("Auto (use all CPU cores)");
    threadCountSpin_->setValue(std::clamp(AppSettings::renderThreadCount(), 0, maxThreads));
    perfForm->addRow("Render threads", threadCountSpin_);
    layout->addWidget(perfGroup);

    auto* previewGroup = new QGroupBox("Preview", tab);
    auto* previewForm = new QFormLayout(previewGroup);
    previewQualityCombo_ = new QComboBox(previewGroup);
    previewQualityCombo_->setObjectName("previewQualityCombo");
    int initialIndex = 1; // Medium, if the stored density doesn't match a preset exactly
    const double currentDensity = AppSettings::previewSampleDensity();
    for (int i = 0; i < static_cast<int>(std::size(kPreviewQualityPresets)); ++i) {
        previewQualityCombo_->addItem(kPreviewQualityPresets[i].name);
        if (kPreviewQualityPresets[i].density == currentDensity) initialIndex = i;
    }
    previewQualityCombo_->setCurrentIndex(initialIndex);
    previewForm->addRow("Live preview quality (Editor, Adjust, Mutate, ...)", previewQualityCombo_);
    layout->addWidget(previewGroup);

    layout->addStretch(1);
    return tab;
}

QWidget* OptionsDialog::buildRandomTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    auto* xformsGroup = new QGroupBox("New Random Flame", tab);
    auto* xformsForm = new QFormLayout(xformsGroup);
    randomMinXformsSpin_ = new QSpinBox(xformsGroup);
    randomMinXformsSpin_->setObjectName("randomMinXformsSpin");
    randomMinXformsSpin_->setRange(1, 100);
    randomMinXformsSpin_->setValue(AppSettings::randomMinXforms());
    randomMaxXformsSpin_ = new QSpinBox(xformsGroup);
    randomMaxXformsSpin_->setObjectName("randomMaxXformsSpin");
    randomMaxXformsSpin_->setRange(1, 100);
    randomMaxXformsSpin_->setValue(AppSettings::randomMaxXforms());
    xformsForm->addRow("Minimum transforms", randomMinXformsSpin_);
    xformsForm->addRow("Maximum transforms", randomMaxXformsSpin_);

    gradientSourceCombo_ = new QComboBox(xformsGroup);
    gradientSourceCombo_->setObjectName("gradientSourceCombo");
    // Ordinal-for-ordinal with RandomFlame.h's RandomGradientSource enum -
    // see AppSettings.h's randomGradientSource() doc comment.
    gradientSourceCombo_->addItem("Random preset gradient");
    gradientSourceCombo_->addItem("Current gradient");
    gradientSourceCombo_->addItem("Calculated random gradient");
    gradientSourceCombo_->setCurrentIndex(std::clamp(AppSettings::randomGradientSource(), 0, 2));
    xformsForm->addRow("Gradient source", gradientSourceCombo_);

    forcedVariationCombo_ = new QComboBox(xformsGroup);
    forcedVariationCombo_->setObjectName("forcedVariationCombo");
    forcedVariationCombo_->addItem("Random");
    const auto& registry = apo::VariationRegistry::instance();
    for (int i = 0; i < registry.nrVar(); ++i) forcedVariationCombo_->addItem(QString::fromStdString(registry.varName(i)));
    const QString forcedName = AppSettings::randomForcedVariationName();
    int forcedIndex = 0; // "Random", if the stored name doesn't match any current variation
    if (!forcedName.isEmpty()) {
        const int found = forcedVariationCombo_->findText(forcedName);
        if (found >= 0) forcedIndex = found;
    }
    forcedVariationCombo_->setCurrentIndex(forcedIndex);
    xformsForm->addRow("Variation", forcedVariationCombo_);

    layout->addWidget(xformsGroup);

    auto* batchGroup = new QGroupBox("Random Batch", tab);
    auto* batchForm = new QFormLayout(batchGroup);
    batchSizeSpin_ = new QSpinBox(batchGroup);
    batchSizeSpin_->setObjectName("batchSizeSpin");
    batchSizeSpin_->setRange(1, 300);
    batchSizeSpin_->setValue(AppSettings::randomBatchSize());
    batchTitlePrefixEdit_ = new QLineEdit(batchGroup);
    batchTitlePrefixEdit_->setObjectName("batchTitlePrefixEdit");
    batchTitlePrefixEdit_->setText(AppSettings::randomBatchTitlePrefix());
    keepBackgroundCheck_ = new QCheckBox("Keep background color", batchGroup);
    keepBackgroundCheck_->setObjectName("keepBackgroundCheck");
    keepBackgroundCheck_->setChecked(AppSettings::randomKeepBackground());
    batchForm->addRow("Default batch size", batchSizeSpin_);
    batchForm->addRow("Title prefix", batchTitlePrefixEdit_);
    batchForm->addRow(keepBackgroundCheck_);
    layout->addWidget(batchGroup);

    auto* mutationGroup = new QGroupBox("Mutation", tab);
    auto* mutationForm = new QFormLayout(mutationGroup);
    mutationMinXformsSpin_ = new QSpinBox(mutationGroup);
    mutationMinXformsSpin_->setObjectName("mutationMinXformsSpin");
    mutationMinXformsSpin_->setRange(1, 100);
    mutationMinXformsSpin_->setValue(AppSettings::mutationMinXforms());
    mutationMaxXformsSpin_ = new QSpinBox(mutationGroup);
    mutationMaxXformsSpin_->setObjectName("mutationMaxXformsSpin");
    mutationMaxXformsSpin_->setRange(1, 100);
    mutationMaxXformsSpin_->setValue(AppSettings::mutationMaxXforms());
    mutationForm->addRow("Minimum transforms", mutationMinXformsSpin_);
    mutationForm->addRow("Maximum transforms", mutationMaxXformsSpin_);
    layout->addWidget(mutationGroup);

    auto* symmetryGroup = new QGroupBox("Forced Symmetry (on random generation)", tab);
    auto* symmetryForm = new QFormLayout(symmetryGroup);
    symmetryTypeCombo_ = new QComboBox(symmetryGroup);
    symmetryTypeCombo_->setObjectName("symmetryTypeCombo");
    for (const char* name : kSymmetryTypeNames) symmetryTypeCombo_->addItem(name);
    symmetryTypeCombo_->setCurrentIndex(
        std::clamp(AppSettings::randomSymmetryType(), 0, static_cast<int>(std::size(kSymmetryTypeNames)) - 1));
    symmetryOrderSpin_ = new QSpinBox(symmetryGroup);
    symmetryOrderSpin_->setObjectName("symmetryOrderSpin");
    symmetryOrderSpin_->setRange(2, 2000);
    symmetryOrderSpin_->setValue(AppSettings::randomSymmetryOrder());
    symmetryForm->addRow("Type", symmetryTypeCombo_);
    symmetryForm->addRow("Order", symmetryOrderSpin_);
    layout->addWidget(symmetryGroup);

    // Blending is ignored once "Variation" above is forced to something
    // other than Random (a forced variation is always exactly that one,
    // matching MutationOps::randomizeXforms's own forced-variation
    // short-circuit) - not disabled/greyed here since forcedVariationCombo_
    // can itself change after this group is built, and this dialog has no
    // signal wiring between tabs elsewhere either.
    auto* blendGroup = new QGroupBox("Variation Blending (Random only)", tab);
    auto* blendForm = new QFormLayout(blendGroup);
    minVariationsPerXformSpin_ = new QSpinBox(blendGroup);
    minVariationsPerXformSpin_->setObjectName("minVariationsPerXformSpin");
    minVariationsPerXformSpin_->setRange(1, 8);
    minVariationsPerXformSpin_->setValue(AppSettings::randomMinVariationsPerXform());
    maxVariationsPerXformSpin_ = new QSpinBox(blendGroup);
    maxVariationsPerXformSpin_->setObjectName("maxVariationsPerXformSpin");
    maxVariationsPerXformSpin_->setRange(1, 8);
    maxVariationsPerXformSpin_->setValue(AppSettings::randomMaxVariationsPerXform());
    blendForm->addRow("Minimum variations per transform", minVariationsPerXformSpin_);
    blendForm->addRow("Maximum variations per transform", maxVariationsPerXformSpin_);

    variationWeightMinSpin_ = new QDoubleSpinBox(blendGroup);
    variationWeightMinSpin_->setObjectName("variationWeightMinSpin");
    variationWeightMinSpin_->setRange(0.01, 2.0);
    variationWeightMinSpin_->setSingleStep(0.05);
    variationWeightMinSpin_->setValue(AppSettings::randomVariationWeightMin());
    variationWeightMaxSpin_ = new QDoubleSpinBox(blendGroup);
    variationWeightMaxSpin_->setObjectName("variationWeightMaxSpin");
    variationWeightMaxSpin_->setRange(0.01, 2.0);
    variationWeightMaxSpin_->setSingleStep(0.05);
    variationWeightMaxSpin_->setValue(AppSettings::randomVariationWeightMax());
    blendForm->addRow("Minimum weight", variationWeightMinSpin_);
    blendForm->addRow("Maximum weight", variationWeightMaxSpin_);

    randomizeVariationParamsCheck_ = new QCheckBox("Randomize variation parameters", blendGroup);
    randomizeVariationParamsCheck_->setObjectName("randomizeVariationParamsCheck");
    randomizeVariationParamsCheck_->setChecked(AppSettings::randomizeVariationParameters());
    blendForm->addRow(randomizeVariationParamsCheck_);

    variationParameterStrengthSpin_ = new QDoubleSpinBox(blendGroup);
    variationParameterStrengthSpin_->setObjectName("variationParameterStrengthSpin");
    variationParameterStrengthSpin_->setRange(0.1, 3.0);
    variationParameterStrengthSpin_->setSingleStep(0.1);
    variationParameterStrengthSpin_->setValue(AppSettings::randomVariationParameterStrength());
    blendForm->addRow("Parameter randomization strength", variationParameterStrengthSpin_);
    layout->addWidget(blendGroup);

    layout->addStretch(1);
    return tab;
}

QWidget* OptionsDialog::buildVariationsTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    auto* label = new QLabel("Unchecked variations are never picked for a randomly-generated topology (New "
                              "Random Batch, or Mutate's \"Random\" trend) - explicitly forcing one still works "
                              "regardless.",
                              tab);
    label->setWordWrap(true);
    layout->addWidget(label);

    variationsList_ = new QListWidget(tab);
    variationsList_->setObjectName("variationsList");
    const auto& registry = apo::VariationRegistry::instance();
    const QStringList disabled = AppSettings::disabledVariationNames();
    for (int i = 0; i < registry.nrVar(); ++i) {
        const QString name = QString::fromStdString(registry.varName(i));
        auto* item = new QListWidgetItem(name, variationsList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(disabled.contains(name) ? Qt::Unchecked : Qt::Checked);
    }
    layout->addWidget(variationsList_, /*stretch=*/1);

    auto* buttonRow = new QHBoxLayout();
    auto* setAllButton = new QPushButton("Set All", tab);
    setAllButton->setObjectName("variationsSetAllButton");
    connect(setAllButton, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < variationsList_->count(); ++i) variationsList_->item(i)->setCheckState(Qt::Checked);
    });
    auto* clearAllButton = new QPushButton("Clear All", tab);
    clearAllButton->setObjectName("variationsClearAllButton");
    connect(clearAllButton, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < variationsList_->count(); ++i) variationsList_->item(i)->setCheckState(Qt::Unchecked);
    });
    buttonRow->addWidget(setAllButton);
    buttonRow->addWidget(clearAllButton);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    return tab;
}

QWidget* OptionsDialog::buildNewFlameTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    auto* coloringGroup = new QGroupBox("Coloring", tab);
    auto* coloringForm = new QFormLayout(coloringGroup);
    defaultGammaSpin_ = new QDoubleSpinBox(coloringGroup);
    defaultGammaSpin_->setObjectName("defaultGammaSpin");
    defaultGammaSpin_->setRange(0.01, 20);
    defaultGammaSpin_->setDecimals(3);
    defaultGammaSpin_->setValue(AppSettings::defaultGamma());
    defaultBrightnessSpin_ = new QDoubleSpinBox(coloringGroup);
    defaultBrightnessSpin_->setObjectName("defaultBrightnessSpin");
    defaultBrightnessSpin_->setRange(0.01, 100);
    defaultBrightnessSpin_->setDecimals(3);
    defaultBrightnessSpin_->setValue(AppSettings::defaultBrightness());
    defaultVibrancySpin_ = new QDoubleSpinBox(coloringGroup);
    defaultVibrancySpin_->setObjectName("defaultVibrancySpin");
    defaultVibrancySpin_->setRange(0, 50);
    defaultVibrancySpin_->setDecimals(3);
    defaultVibrancySpin_->setValue(AppSettings::defaultVibrancy());
    defaultGammaThresholdSpin_ = new QDoubleSpinBox(coloringGroup);
    defaultGammaThresholdSpin_->setObjectName("defaultGammaThresholdSpin");
    defaultGammaThresholdSpin_->setRange(0, 1);
    defaultGammaThresholdSpin_->setDecimals(4);
    defaultGammaThresholdSpin_->setValue(AppSettings::defaultGammaThreshold());
    coloringForm->addRow("Gamma", defaultGammaSpin_);
    coloringForm->addRow("Brightness", defaultBrightnessSpin_);
    coloringForm->addRow("Vibrancy", defaultVibrancySpin_);
    coloringForm->addRow("Gamma threshold", defaultGammaThresholdSpin_);
    layout->addWidget(coloringGroup);

    auto* renderingGroup = new QGroupBox("Rendering", tab);
    auto* renderingForm = new QFormLayout(renderingGroup);
    defaultSampleDensitySpin_ = new QDoubleSpinBox(renderingGroup);
    defaultSampleDensitySpin_->setObjectName("defaultSampleDensitySpin");
    defaultSampleDensitySpin_->setRange(1, 1000000);
    defaultSampleDensitySpin_->setDecimals(1);
    defaultSampleDensitySpin_->setValue(AppSettings::defaultSampleDensity());
    defaultOversampleSpin_ = new QSpinBox(renderingGroup);
    defaultOversampleSpin_->setObjectName("defaultOversampleSpin");
    defaultOversampleSpin_->setRange(1, 8);
    defaultOversampleSpin_->setValue(AppSettings::defaultOversample());
    defaultFilterRadiusSpin_ = new QDoubleSpinBox(renderingGroup);
    defaultFilterRadiusSpin_->setObjectName("defaultFilterRadiusSpin");
    defaultFilterRadiusSpin_->setRange(0, 50);
    defaultFilterRadiusSpin_->setDecimals(2);
    defaultFilterRadiusSpin_->setValue(AppSettings::defaultFilterRadius());
    renderingForm->addRow("Sample density", defaultSampleDensitySpin_);
    renderingForm->addRow("Oversample", defaultOversampleSpin_);
    renderingForm->addRow("Filter radius", defaultFilterRadiusSpin_);
    layout->addWidget(renderingGroup);

    layout->addStretch(1);
    return tab;
}

void OptionsDialog::applyAndAccept() {
    AppSettings::setRenderThreadCount(threadCountSpin_->value());

    const int index = previewQualityCombo_->currentIndex();
    if (index >= 0 && index < static_cast<int>(std::size(kPreviewQualityPresets))) {
        AppSettings::setPreviewSampleDensity(kPreviewQualityPresets[index].density);
    }

    AppSettings::setRandomMinXforms(randomMinXformsSpin_->value());
    AppSettings::setRandomMaxXforms(randomMaxXformsSpin_->value());
    AppSettings::setRandomBatchSize(batchSizeSpin_->value());
    AppSettings::setRandomBatchTitlePrefix(batchTitlePrefixEdit_->text());
    AppSettings::setRandomKeepBackground(keepBackgroundCheck_->isChecked());
    AppSettings::setMutationMinXforms(mutationMinXformsSpin_->value());
    AppSettings::setMutationMaxXforms(mutationMaxXformsSpin_->value());
    AppSettings::setRandomSymmetryType(symmetryTypeCombo_->currentIndex());
    AppSettings::setRandomSymmetryOrder(symmetryOrderSpin_->value());
    AppSettings::setRandomMinVariationsPerXform(minVariationsPerXformSpin_->value());
    AppSettings::setRandomMaxVariationsPerXform(maxVariationsPerXformSpin_->value());
    AppSettings::setRandomVariationWeightMin(variationWeightMinSpin_->value());
    AppSettings::setRandomVariationWeightMax(variationWeightMaxSpin_->value());
    AppSettings::setRandomizeVariationParameters(randomizeVariationParamsCheck_->isChecked());
    AppSettings::setRandomVariationParameterStrength(variationParameterStrengthSpin_->value());
    AppSettings::setRandomGradientSource(gradientSourceCombo_->currentIndex());
    AppSettings::setRandomForcedVariationName(forcedVariationCombo_->currentIndex() == 0
                                                   ? QString()
                                                   : forcedVariationCombo_->currentText());

    QStringList disabledVariations;
    for (int i = 0; i < variationsList_->count(); ++i) {
        if (variationsList_->item(i)->checkState() == Qt::Unchecked) disabledVariations << variationsList_->item(i)->text();
    }
    AppSettings::setDisabledVariationNames(disabledVariations);

    AppSettings::setDefaultGamma(defaultGammaSpin_->value());
    AppSettings::setDefaultBrightness(defaultBrightnessSpin_->value());
    AppSettings::setDefaultVibrancy(defaultVibrancySpin_->value());
    AppSettings::setDefaultGammaThreshold(defaultGammaThresholdSpin_->value());
    AppSettings::setDefaultSampleDensity(defaultSampleDensitySpin_->value());
    AppSettings::setDefaultOversample(defaultOversampleSpin_->value());
    AppSettings::setDefaultFilterRadius(defaultFilterRadiusSpin_->value());

    accept();
}

void OptionsDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

void OptionsDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
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
