#pragma once

#include <QDialog>
#include <QString>

class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QListWidget;
class QShowEvent;

namespace apo::ui {

// Port of Options.pas (1,483 lines, 9 tabs) - deliberately scoped down to
// just the settings an existing feature of this port actually reads, per
// the "good performance, not a faithful copy" project direction (see
// AppSettings.h's header comment for the full rationale and per-tab
// scoping notes). Kept, across 4 tabs (collapsed from the original's 9 -
// General/Editor/Display/Random/Variations/Gradient/UPR/Sheep/Environment -
// down to just what maps to a real, already-ported feature):
//   - General: render thread count and live preview quality (shared by
//     Adjust/Mutate/Curves/PostProcess *and* the Editor's own canvas
//     preview - see EditorWindow.h's class comment).
//   - Random: new-random-flame/mutation transform-count ranges, batch
//     defaults, forced symmetry, gradient source, forced variation, and
//     variation-blending (how many variations combine per xform, their
//     weight range, and whether their own parameters get randomized) on
//     random generation, all consumed by MainWindow::
//     onNewRandomBatchTriggered; mutation transform-count range also
//     consumed by MutateDialog.
//   - Variations: the enable/disable checklist restricting which
//     variations a "Random" topology draw can land on - consumed by
//     MainWindow's random batch generation AND MutateDialog's own
//     Trend=Random path (both via MutationOps::randomizeXforms/
//     setXformsVariation's eligibleVariations parameter).
//   - New Flame: the tone-mapping/render defaults (gamma/brightness/
//     vibrancy/gamma threshold/sample density/oversample/filter radius)
//     applied to a brand-new blank or randomly generated flame.
// Dropped: sound, language/i18n, editor guideline overlays and colors,
// confirm-on-delete/exit/stop-render (this port already always confirms
// Delete - see MainWindow.h's Rename/Delete comment - and has no unsaved-
// changes tracking to gate exit/stop-render on), PNG transparency default,
// JPEG quality, random-gradient-generation tuning (the node-count/hue/
// saturation/luminance ranges GradientOps.h's randomGradient() uses -
// hardcoded there, see its own comment), and gradient file path/Sheep/
// UPR/flam3/Chaotica interop (external services and programs, out of
// scope).
//
// A plain OK/Cancel settings form (via AppSettings.h/QSettings), not a
// live-preview dialog like Adjust/Render - changes only take effect once
// OK is pressed.
class OptionsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget* parent = nullptr);

    // Testing/verification hook - same idea as the other dialogs'
    // setAutoScreenshot (e.g. RenderDialog.h), for the --options CLI flag.
    // Unlike those, this dialog is normally used modally (exec()), so the
    // CLI test path calls show() instead - a screenshot is taken shortly
    // after the dialog first becomes visible rather than after some
    // asynchronous render settles, since nothing here is async.
    void setAutoScreenshot(const QString& path, bool exitAfter);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void applyAndAccept();

private:
    QWidget* buildGeneralTab();
    QWidget* buildRandomTab();
    QWidget* buildVariationsTab();
    QWidget* buildNewFlameTab();

    QSpinBox* threadCountSpin_ = nullptr;
    QComboBox* previewQualityCombo_ = nullptr;

    // Random tab
    QSpinBox* randomMinXformsSpin_ = nullptr;
    QSpinBox* randomMaxXformsSpin_ = nullptr;
    QComboBox* gradientSourceCombo_ = nullptr;
    QComboBox* forcedVariationCombo_ = nullptr;
    QSpinBox* batchSizeSpin_ = nullptr;
    QLineEdit* batchTitlePrefixEdit_ = nullptr;
    QCheckBox* keepBackgroundCheck_ = nullptr;
    QSpinBox* mutationMinXformsSpin_ = nullptr;
    QSpinBox* mutationMaxXformsSpin_ = nullptr;
    QComboBox* symmetryTypeCombo_ = nullptr;
    QSpinBox* symmetryOrderSpin_ = nullptr;
    QSpinBox* minVariationsPerXformSpin_ = nullptr;
    QSpinBox* maxVariationsPerXformSpin_ = nullptr;
    QDoubleSpinBox* variationWeightMinSpin_ = nullptr;
    QDoubleSpinBox* variationWeightMaxSpin_ = nullptr;
    QCheckBox* randomizeVariationParamsCheck_ = nullptr;
    QDoubleSpinBox* variationParameterStrengthSpin_ = nullptr;

    // Variations tab
    QListWidget* variationsList_ = nullptr;

    // New Flame tab
    QDoubleSpinBox* defaultGammaSpin_ = nullptr;
    QDoubleSpinBox* defaultBrightnessSpin_ = nullptr;
    QDoubleSpinBox* defaultVibrancySpin_ = nullptr;
    QDoubleSpinBox* defaultGammaThresholdSpin_ = nullptr;
    QDoubleSpinBox* defaultSampleDensitySpin_ = nullptr;
    QSpinBox* defaultOversampleSpin_ = nullptr;
    QDoubleSpinBox* defaultFilterRadiusSpin_ = nullptr;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
    bool autoScreenshotFired_ = false;
};

} // namespace apo::ui
