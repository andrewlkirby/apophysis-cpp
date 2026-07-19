// Real-widget interaction tests for OptionsDialog (src/ui/OptionsDialog.h)
// and its AppSettings.h backing store. Verifies: controls are seeded from
// the persisted AppSettings values, clicking OK writes new values back to
// AppSettings, clicking Cancel discards edits without persisting them, and
// the thread-count spin box's Auto special value maps to 0 (matching
// apo::Renderer::render()'s own threadCount=0 meaning). Mirrors
// adjust_dialog_interaction_test.cpp/render_dialog_interaction_test.cpp's
// approach (QTest event injection against the real compiled widget).
//
// AppSettings is QSettings-backed (real registry/ini storage under this
// machine's "Apophysis 7X" org/app name, set in main.cpp) - every test
// saves the values it finds on entry and restores them on exit via
// SettingsGuard, so running this test suite never permanently changes a
// developer's actual saved preferences.

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>

#include "../TestHelpers.h"
#include "AppSettings.h"
#include "OptionsDialog.h"
#include "core/VariationRegistry.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

struct SettingsGuard {
    int savedThreads = apo::ui::AppSettings::renderThreadCount();
    double savedDensity = apo::ui::AppSettings::previewSampleDensity();
    int savedRandomMin = apo::ui::AppSettings::randomMinXforms();
    int savedRandomMax = apo::ui::AppSettings::randomMaxXforms();
    int savedBatchSize = apo::ui::AppSettings::randomBatchSize();
    QString savedBatchPrefix = apo::ui::AppSettings::randomBatchTitlePrefix();
    bool savedKeepBackground = apo::ui::AppSettings::randomKeepBackground();
    int savedMutationMin = apo::ui::AppSettings::mutationMinXforms();
    int savedMutationMax = apo::ui::AppSettings::mutationMaxXforms();
    int savedSymType = apo::ui::AppSettings::randomSymmetryType();
    int savedSymOrder = apo::ui::AppSettings::randomSymmetryOrder();
    int savedMinVariationsPerXform = apo::ui::AppSettings::randomMinVariationsPerXform();
    int savedMaxVariationsPerXform = apo::ui::AppSettings::randomMaxVariationsPerXform();
    double savedVariationWeightMin = apo::ui::AppSettings::randomVariationWeightMin();
    double savedVariationWeightMax = apo::ui::AppSettings::randomVariationWeightMax();
    bool savedRandomizeVariationParameters = apo::ui::AppSettings::randomizeVariationParameters();
    double savedVariationParameterStrength = apo::ui::AppSettings::randomVariationParameterStrength();
    int savedGradientSource = apo::ui::AppSettings::randomGradientSource();
    QString savedForcedVariation = apo::ui::AppSettings::randomForcedVariationName();
    QStringList savedDisabledVariations = apo::ui::AppSettings::disabledVariationNames();
    double savedGamma = apo::ui::AppSettings::defaultGamma();
    double savedBrightness = apo::ui::AppSettings::defaultBrightness();
    double savedVibrancy = apo::ui::AppSettings::defaultVibrancy();
    double savedGammaThreshold = apo::ui::AppSettings::defaultGammaThreshold();
    double savedSampleDensity = apo::ui::AppSettings::defaultSampleDensity();
    int savedOversample = apo::ui::AppSettings::defaultOversample();
    double savedFilterRadius = apo::ui::AppSettings::defaultFilterRadius();

    ~SettingsGuard() {
        apo::ui::AppSettings::setRenderThreadCount(savedThreads);
        apo::ui::AppSettings::setPreviewSampleDensity(savedDensity);
        apo::ui::AppSettings::setRandomMinXforms(savedRandomMin);
        apo::ui::AppSettings::setRandomMaxXforms(savedRandomMax);
        apo::ui::AppSettings::setRandomBatchSize(savedBatchSize);
        apo::ui::AppSettings::setRandomBatchTitlePrefix(savedBatchPrefix);
        apo::ui::AppSettings::setRandomKeepBackground(savedKeepBackground);
        apo::ui::AppSettings::setMutationMinXforms(savedMutationMin);
        apo::ui::AppSettings::setMutationMaxXforms(savedMutationMax);
        apo::ui::AppSettings::setRandomSymmetryType(savedSymType);
        apo::ui::AppSettings::setRandomSymmetryOrder(savedSymOrder);
        apo::ui::AppSettings::setRandomMinVariationsPerXform(savedMinVariationsPerXform);
        apo::ui::AppSettings::setRandomMaxVariationsPerXform(savedMaxVariationsPerXform);
        apo::ui::AppSettings::setRandomVariationWeightMin(savedVariationWeightMin);
        apo::ui::AppSettings::setRandomVariationWeightMax(savedVariationWeightMax);
        apo::ui::AppSettings::setRandomizeVariationParameters(savedRandomizeVariationParameters);
        apo::ui::AppSettings::setRandomVariationParameterStrength(savedVariationParameterStrength);
        apo::ui::AppSettings::setRandomGradientSource(savedGradientSource);
        apo::ui::AppSettings::setRandomForcedVariationName(savedForcedVariation);
        apo::ui::AppSettings::setDisabledVariationNames(savedDisabledVariations);
        apo::ui::AppSettings::setDefaultGamma(savedGamma);
        apo::ui::AppSettings::setDefaultBrightness(savedBrightness);
        apo::ui::AppSettings::setDefaultVibrancy(savedVibrancy);
        apo::ui::AppSettings::setDefaultGammaThreshold(savedGammaThreshold);
        apo::ui::AppSettings::setDefaultSampleDensity(savedSampleDensity);
        apo::ui::AppSettings::setDefaultOversample(savedOversample);
        apo::ui::AppSettings::setDefaultFilterRadius(savedFilterRadius);
    }
};

void testControlsSeededFromAppSettings() {
    SettingsGuard guard;
    apo::ui::AppSettings::setRenderThreadCount(3);
    apo::ui::AppSettings::setPreviewSampleDensity(16.0); // "High"

    auto* dialog = new apo::ui::OptionsDialog();
    auto* threadSpin = dialog->findChild<QSpinBox*>("threadCountSpin");
    auto* qualityCombo = dialog->findChild<QComboBox*>("previewQualityCombo");
    if (!check(threadSpin && qualityCombo, "OptionsDialog exposes threadCountSpin and previewQualityCombo")) {
        delete dialog;
        return;
    }

    check(threadSpin->value() == 3, "threadCountSpin is seeded from AppSettings::renderThreadCount()");
    check(qualityCombo->currentText() == "High", "previewQualityCombo is seeded to the preset matching the stored density");

    delete dialog;
}

void testThreadCountAutoIsZero() {
    SettingsGuard guard;
    auto* dialog = new apo::ui::OptionsDialog();
    auto* threadSpin = dialog->findChild<QSpinBox*>("threadCountSpin");
    if (!check(threadSpin != nullptr, "threadCountSpin found")) {
        delete dialog;
        return;
    }

    check(threadSpin->minimum() == 0, "threadCountSpin's minimum is 0 (Auto)");
    threadSpin->setValue(0);
    check(!threadSpin->specialValueText().isEmpty() && threadSpin->text() == threadSpin->specialValueText(),
          "at 0, the spin box displays its Auto special value text, matching Renderer::render()'s threadCount=0 meaning");

    delete dialog;
}

void testOkButtonPersistsChangesToAppSettings() {
    SettingsGuard guard;
    apo::ui::AppSettings::setRenderThreadCount(0);
    apo::ui::AppSettings::setPreviewSampleDensity(8.0); // Medium

    auto* dialog = new apo::ui::OptionsDialog();
    auto* threadSpin = dialog->findChild<QSpinBox*>("threadCountSpin");
    auto* qualityCombo = dialog->findChild<QComboBox*>("previewQualityCombo");
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    if (!check(threadSpin && qualityCombo && buttons, "controls and button box found")) {
        delete dialog;
        return;
    }

    threadSpin->setValue(5);
    qualityCombo->setCurrentIndex(0); // Low

    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);

    check(apo::ui::AppSettings::renderThreadCount() == 5, "OK persists the edited thread count to AppSettings");
    check(approxEqual(apo::ui::AppSettings::previewSampleDensity(), 4.0),
          "OK persists the edited preview quality (Low = density 4.0) to AppSettings");

    delete dialog;
}

void testCancelButtonDiscardsChanges() {
    SettingsGuard guard;
    apo::ui::AppSettings::setRenderThreadCount(2);
    apo::ui::AppSettings::setPreviewSampleDensity(8.0); // Medium

    auto* dialog = new apo::ui::OptionsDialog();
    auto* threadSpin = dialog->findChild<QSpinBox*>("threadCountSpin");
    auto* qualityCombo = dialog->findChild<QComboBox*>("previewQualityCombo");
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    if (!check(threadSpin && qualityCombo && buttons, "controls and button box found")) {
        delete dialog;
        return;
    }

    threadSpin->setValue(7);
    qualityCombo->setCurrentIndex(2); // High

    QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel), Qt::LeftButton);

    check(apo::ui::AppSettings::renderThreadCount() == 2, "Cancel leaves AppSettings::renderThreadCount() unchanged");
    check(approxEqual(apo::ui::AppSettings::previewSampleDensity(), 8.0),
          "Cancel leaves AppSettings::previewSampleDensity() unchanged");

    delete dialog;
}

void testRandomTabControlsSeededFromAppSettings() {
    SettingsGuard guard;
    apo::ui::AppSettings::setRandomMinXforms(3);
    apo::ui::AppSettings::setRandomMaxXforms(9);
    apo::ui::AppSettings::setRandomBatchSize(25);
    apo::ui::AppSettings::setRandomBatchTitlePrefix("MyFlames");
    apo::ui::AppSettings::setRandomKeepBackground(true);
    apo::ui::AppSettings::setMutationMinXforms(4);
    apo::ui::AppSettings::setMutationMaxXforms(10);
    apo::ui::AppSettings::setRandomSymmetryType(2); // Rotational
    apo::ui::AppSettings::setRandomSymmetryOrder(7);

    auto* dialog = new apo::ui::OptionsDialog();
    auto* minXforms = dialog->findChild<QSpinBox*>("randomMinXformsSpin");
    auto* maxXforms = dialog->findChild<QSpinBox*>("randomMaxXformsSpin");
    auto* batchSize = dialog->findChild<QSpinBox*>("batchSizeSpin");
    auto* batchPrefix = dialog->findChild<QLineEdit*>("batchTitlePrefixEdit");
    auto* keepBackground = dialog->findChild<QCheckBox*>("keepBackgroundCheck");
    auto* mutationMin = dialog->findChild<QSpinBox*>("mutationMinXformsSpin");
    auto* mutationMax = dialog->findChild<QSpinBox*>("mutationMaxXformsSpin");
    auto* symType = dialog->findChild<QComboBox*>("symmetryTypeCombo");
    auto* symOrder = dialog->findChild<QSpinBox*>("symmetryOrderSpin");
    if (!check(minXforms && maxXforms && batchSize && batchPrefix && keepBackground && mutationMin && mutationMax &&
                   symType && symOrder,
               "every Random tab control is found")) {
        delete dialog;
        return;
    }

    check(minXforms->value() == 3 && maxXforms->value() == 9, "min/max random transform spins are seeded");
    check(batchSize->value() == 25, "batch size spin is seeded");
    check(batchPrefix->text() == "MyFlames", "batch title prefix edit is seeded");
    check(keepBackground->isChecked(), "keep background checkbox is seeded");
    check(mutationMin->value() == 4 && mutationMax->value() == 10, "min/max mutation transform spins are seeded");
    check(symType->currentIndex() == 2 && symType->currentText() == "Rotational",
          "symmetry type combo is seeded to the stored index");
    check(symOrder->value() == 7, "symmetry order spin is seeded");

    delete dialog;
}

void testVariationBlendControlsSeededFromAppSettings() {
    SettingsGuard guard;
    apo::ui::AppSettings::setRandomMinVariationsPerXform(2);
    apo::ui::AppSettings::setRandomMaxVariationsPerXform(4);
    apo::ui::AppSettings::setRandomVariationWeightMin(0.25);
    apo::ui::AppSettings::setRandomVariationWeightMax(0.75);
    apo::ui::AppSettings::setRandomizeVariationParameters(false);
    apo::ui::AppSettings::setRandomVariationParameterStrength(1.5);

    auto* dialog = new apo::ui::OptionsDialog();
    auto* minVariations = dialog->findChild<QSpinBox*>("minVariationsPerXformSpin");
    auto* maxVariations = dialog->findChild<QSpinBox*>("maxVariationsPerXformSpin");
    auto* weightMin = dialog->findChild<QDoubleSpinBox*>("variationWeightMinSpin");
    auto* weightMax = dialog->findChild<QDoubleSpinBox*>("variationWeightMaxSpin");
    auto* randomizeParams = dialog->findChild<QCheckBox*>("randomizeVariationParamsCheck");
    auto* paramStrength = dialog->findChild<QDoubleSpinBox*>("variationParameterStrengthSpin");
    if (!check(minVariations && maxVariations && weightMin && weightMax && randomizeParams && paramStrength,
               "every variation blending control is found")) {
        delete dialog;
        return;
    }

    check(minVariations->value() == 2 && maxVariations->value() == 4,
          "min/max variations-per-transform spins are seeded");
    check(approxEqual(weightMin->value(), 0.25) && approxEqual(weightMax->value(), 0.75),
          "min/max variation weight spins are seeded");
    check(!randomizeParams->isChecked(), "randomize-variation-parameters checkbox is seeded");
    check(approxEqual(paramStrength->value(), 1.5), "parameter randomization strength spin is seeded");

    delete dialog;
}

void testVariationBlendControlsOkPersistsToAppSettings() {
    SettingsGuard guard;

    auto* dialog = new apo::ui::OptionsDialog();
    auto* minVariations = dialog->findChild<QSpinBox*>("minVariationsPerXformSpin");
    auto* maxVariations = dialog->findChild<QSpinBox*>("maxVariationsPerXformSpin");
    auto* weightMin = dialog->findChild<QDoubleSpinBox*>("variationWeightMinSpin");
    auto* weightMax = dialog->findChild<QDoubleSpinBox*>("variationWeightMaxSpin");
    auto* randomizeParams = dialog->findChild<QCheckBox*>("randomizeVariationParamsCheck");
    auto* paramStrength = dialog->findChild<QDoubleSpinBox*>("variationParameterStrengthSpin");
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    if (!check(minVariations && maxVariations && weightMin && weightMax && randomizeParams && paramStrength && buttons,
               "controls found")) {
        delete dialog;
        return;
    }

    minVariations->setValue(3);
    maxVariations->setValue(5);
    weightMin->setValue(0.4);
    weightMax->setValue(0.9);
    randomizeParams->setChecked(!randomizeParams->isChecked());
    const bool toggledParams = randomizeParams->isChecked();
    paramStrength->setValue(2.0);

    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);

    check(apo::ui::AppSettings::randomMinVariationsPerXform() == 3 &&
              apo::ui::AppSettings::randomMaxVariationsPerXform() == 5,
          "OK persists the edited variations-per-transform range");
    check(approxEqual(apo::ui::AppSettings::randomVariationWeightMin(), 0.4) &&
              approxEqual(apo::ui::AppSettings::randomVariationWeightMax(), 0.9),
          "OK persists the edited variation weight range");
    check(apo::ui::AppSettings::randomizeVariationParameters() == toggledParams,
          "OK persists the edited randomize-variation-parameters checkbox");
    check(approxEqual(apo::ui::AppSettings::randomVariationParameterStrength(), 2.0),
          "OK persists the edited parameter randomization strength");

    delete dialog;
}

void testRandomTabOkPersistsToAppSettings() {
    SettingsGuard guard;

    auto* dialog = new apo::ui::OptionsDialog();
    auto* minXforms = dialog->findChild<QSpinBox*>("randomMinXformsSpin");
    auto* maxXforms = dialog->findChild<QSpinBox*>("randomMaxXformsSpin");
    auto* batchPrefix = dialog->findChild<QLineEdit*>("batchTitlePrefixEdit");
    auto* keepBackground = dialog->findChild<QCheckBox*>("keepBackgroundCheck");
    auto* symType = dialog->findChild<QComboBox*>("symmetryTypeCombo");
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    if (!check(minXforms && maxXforms && batchPrefix && keepBackground && symType && buttons, "controls found")) {
        delete dialog;
        return;
    }

    minXforms->setValue(5);
    maxXforms->setValue(11);
    batchPrefix->setText("Batch Title");
    keepBackground->setChecked(true);
    symType->setCurrentIndex(3); // Dihedral

    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);

    check(apo::ui::AppSettings::randomMinXforms() == 5 && apo::ui::AppSettings::randomMaxXforms() == 11,
          "OK persists the edited random transform-count range");
    check(apo::ui::AppSettings::randomBatchTitlePrefix() == "Batch Title", "OK persists the edited batch title prefix");
    check(apo::ui::AppSettings::randomKeepBackground(), "OK persists the edited keep-background checkbox");
    check(apo::ui::AppSettings::randomSymmetryType() == 3, "OK persists the edited symmetry type");

    delete dialog;
}

void testGradientSourceAndForcedVariationSeededAndPersisted() {
    SettingsGuard guard;
    const auto& registry = apo::VariationRegistry::instance();
    const QString variationName = QString::fromStdString(registry.varName(5)); // horseshoe
    apo::ui::AppSettings::setRandomGradientSource(0); // Random preset
    apo::ui::AppSettings::setRandomForcedVariationName(variationName);

    auto* dialog = new apo::ui::OptionsDialog();
    auto* gradientSource = dialog->findChild<QComboBox*>("gradientSourceCombo");
    auto* forcedVariation = dialog->findChild<QComboBox*>("forcedVariationCombo");
    if (!check(gradientSource && forcedVariation, "gradientSourceCombo and forcedVariationCombo found")) {
        delete dialog;
        return;
    }

    check(gradientSource->currentIndex() == 0, "gradientSourceCombo is seeded from AppSettings::randomGradientSource()");
    check(forcedVariation->currentText() == variationName,
          "forcedVariationCombo is seeded to the stored variation name");

    gradientSource->setCurrentIndex(2); // Calculated
    forcedVariation->setCurrentIndex(0); // Random
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);

    check(apo::ui::AppSettings::randomGradientSource() == 2, "OK persists the edited gradient source");
    check(apo::ui::AppSettings::randomForcedVariationName().isEmpty(),
          "selecting \"Random\" persists an empty forced-variation name");

    delete dialog;
}

void testVariationsTabSeededFromAppSettings() {
    SettingsGuard guard;
    const auto& registry = apo::VariationRegistry::instance();
    const QString disabledName = QString::fromStdString(registry.varName(3)); // spherical
    apo::ui::AppSettings::setDisabledVariationNames({disabledName});

    auto* dialog = new apo::ui::OptionsDialog();
    auto* list = dialog->findChild<QListWidget*>("variationsList");
    if (!check(list != nullptr, "variationsList found")) {
        delete dialog;
        return;
    }

    check(list->count() == registry.nrVar(), "the list has exactly one row per registered variation");
    bool foundDisabledUnchecked = false, everythingElseChecked = true;
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* item = list->item(i);
        if (item->text() == disabledName) {
            foundDisabledUnchecked = item->checkState() == Qt::Unchecked;
        } else if (item->checkState() != Qt::Checked) {
            everythingElseChecked = false;
        }
    }
    check(foundDisabledUnchecked, "the previously-disabled variation is seeded unchecked");
    check(everythingElseChecked, "every other variation is seeded checked (enabled by default)");

    delete dialog;
}

void testVariationsTabSetAllAndClearAllButtons() {
    SettingsGuard guard;
    apo::ui::AppSettings::setDisabledVariationNames(
        {QString::fromStdString(apo::VariationRegistry::instance().varName(0))});

    auto* dialog = new apo::ui::OptionsDialog();
    auto* list = dialog->findChild<QListWidget*>("variationsList");
    auto* clearAllButton = dialog->findChild<QPushButton*>("variationsClearAllButton");
    auto* setAllButton = dialog->findChild<QPushButton*>("variationsSetAllButton");
    if (!check(list && clearAllButton && setAllButton, "variationsList and its buttons found")) {
        delete dialog;
        return;
    }

    QTest::mouseClick(clearAllButton, Qt::LeftButton);
    bool everyItemUnchecked = true;
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->checkState() != Qt::Unchecked) everyItemUnchecked = false;
    }
    check(everyItemUnchecked, "Clear All unchecks every variation");

    QTest::mouseClick(setAllButton, Qt::LeftButton);
    bool everyItemChecked = true;
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->checkState() != Qt::Checked) everyItemChecked = false;
    }
    check(everyItemChecked, "Set All checks every variation");

    delete dialog;
}

void testVariationsTabOkPersistsUncheckedItemsAsDisabled() {
    SettingsGuard guard;
    apo::ui::AppSettings::setDisabledVariationNames({});

    auto* dialog = new apo::ui::OptionsDialog();
    auto* list = dialog->findChild<QListWidget*>("variationsList");
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    if (!check(list && buttons, "variationsList and button box found")) {
        delete dialog;
        return;
    }

    const QString targetName = list->item(4)->text();
    list->item(4)->setCheckState(Qt::Unchecked);

    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);

    const QStringList disabled = apo::ui::AppSettings::disabledVariationNames();
    check(disabled.size() == 1 && disabled.first() == targetName,
          "OK persists exactly the unchecked variation names as the disabled set");

    delete dialog;
}

void testNewFlameTabControlsSeededAndPersisted() {
    SettingsGuard guard;
    apo::ui::AppSettings::setDefaultGamma(3.5);
    apo::ui::AppSettings::setDefaultBrightness(6.0);
    apo::ui::AppSettings::setDefaultSampleDensity(120.0);
    apo::ui::AppSettings::setDefaultOversample(3);

    auto* dialog = new apo::ui::OptionsDialog();
    auto* gamma = dialog->findChild<QDoubleSpinBox*>("defaultGammaSpin");
    auto* brightness = dialog->findChild<QDoubleSpinBox*>("defaultBrightnessSpin");
    auto* density = dialog->findChild<QDoubleSpinBox*>("defaultSampleDensitySpin");
    auto* oversample = dialog->findChild<QSpinBox*>("defaultOversampleSpin");
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    if (!check(gamma && brightness && density && oversample && buttons, "New Flame tab controls found")) {
        delete dialog;
        return;
    }

    check(approxEqual(gamma->value(), 3.5) && approxEqual(brightness->value(), 6.0) &&
              approxEqual(density->value(), 120.0) && oversample->value() == 3,
          "New Flame tab controls are seeded from AppSettings");

    gamma->setValue(2.2);
    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
    check(approxEqual(apo::ui::AppSettings::defaultGamma(), 2.2), "OK persists an edited New Flame default");

    delete dialog;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setApplicationName("Apophysis 7X");
    QApplication::setOrganizationName("Apophysis 7X");

    testControlsSeededFromAppSettings();
    testThreadCountAutoIsZero();
    testOkButtonPersistsChangesToAppSettings();
    testCancelButtonDiscardsChanges();
    testRandomTabControlsSeededFromAppSettings();
    testRandomTabOkPersistsToAppSettings();
    testVariationBlendControlsSeededFromAppSettings();
    testVariationBlendControlsOkPersistsToAppSettings();
    testGradientSourceAndForcedVariationSeededAndPersisted();
    testVariationsTabSeededFromAppSettings();
    testVariationsTabSetAllAndClearAllButtons();
    testVariationsTabOkPersistsUncheckedItemsAsDisabled();
    testNewFlameTabControlsSeededAndPersisted();

    return apo_test::reportAndExit();
}
