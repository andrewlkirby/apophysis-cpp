#include "AppSettings.h"

#include <QSettings>

#include "core/VariationRegistry.h"

namespace apo::ui::AppSettings {

namespace {
constexpr int kDefaultRenderThreadCount = 0; // Auto - see Renderer::render()
constexpr double kDefaultPreviewSampleDensity = 8.0; // AdjustDialog's prior hardcoded constant

// Matches RandomFlame.h's prior hardcoded kDefaultMinRandomXforms/
// kDefaultMaxRandomXforms exactly.
constexpr int kDefaultRandomMinXforms = 2;
constexpr int kDefaultRandomMaxXforms = 6;
constexpr int kDefaultRandomBatchSize = 10; // matches Options.dfm's txtBatchSize.Text
constexpr const char* kDefaultRandomBatchTitlePrefix = "Random"; // matches MainWindow.cpp's prior hardcoded prefix
constexpr bool kDefaultRandomKeepBackground = false;
// Matches MutateDialog.cpp's prior hardcoded kDefaultMinXforms/kDefaultMaxXforms.
constexpr int kDefaultMutationMinXforms = 2;
constexpr int kDefaultMutationMaxXforms = 6;
constexpr int kDefaultRandomSymmetryType = 0; // None
constexpr int kDefaultRandomSymmetryOrder = 4; // matches Options.dfm's txtSymOrder.Text
constexpr int kDefaultRandomGradientSource = 2; // Calculated - matches this port's pre-Options-tab behavior
constexpr const char* kDefaultRandomForcedVariationName = ""; // empty = Random

// See AppSettings.h's own comment: no prior hardcoded constant to match, so
// these are chosen to make blending/parameter randomization visible by
// default rather than a hidden opt-in - up to 2 variations blended per
// xform, weighted between 30% and 100%, with each variation's own
// parameters randomized around its class default.
constexpr int kDefaultRandomMinVariationsPerXform = 1;
constexpr int kDefaultRandomMaxVariationsPerXform = 2;
constexpr double kDefaultRandomVariationWeightMin = 0.3;
constexpr double kDefaultRandomVariationWeightMax = 1.0;
constexpr bool kDefaultRandomizeVariationParameters = true;
constexpr double kDefaultRandomVariationParameterStrength = 1.0;

// Gamma/brightness deliberately do NOT match Flame.h's own raw class
// defaults (which stay 1.0, matching ControlPoint.pas's bare constructor) -
// these two instead match Settings.pas/Regstry.pas's own defGamma/
// defBrightness fallback (both `:= 4`, used whenever no `Gamma`/
// `Brightness` registry value exists yet), the original's actual "New
// Flame" default - verified directly against source, not assumed; this
// port previously (wrongly) mirrored the raw class default here instead.
constexpr double kDefaultDefaultGamma = 4.0;
constexpr double kDefaultDefaultBrightness = 4.0;
constexpr double kDefaultDefaultVibrancy = 1.0;
constexpr double kDefaultDefaultGammaThreshold = 0.01;
constexpr double kDefaultDefaultSampleDensity = 50.0;
constexpr int kDefaultDefaultOversample = 1;
constexpr double kDefaultDefaultFilterRadius = 0.5;
} // namespace

int renderThreadCount() {
    return QSettings().value("rendering/threadCount", kDefaultRenderThreadCount).toInt();
}

void setRenderThreadCount(int threads) {
    QSettings().setValue("rendering/threadCount", threads);
}

double previewSampleDensity() {
    return QSettings().value("rendering/previewSampleDensity", kDefaultPreviewSampleDensity).toDouble();
}

void setPreviewSampleDensity(double density) {
    QSettings().setValue("rendering/previewSampleDensity", density);
}

int randomMinXforms() { return QSettings().value("random/minXforms", kDefaultRandomMinXforms).toInt(); }
void setRandomMinXforms(int count) { QSettings().setValue("random/minXforms", count); }
int randomMaxXforms() { return QSettings().value("random/maxXforms", kDefaultRandomMaxXforms).toInt(); }
void setRandomMaxXforms(int count) { QSettings().setValue("random/maxXforms", count); }
int randomBatchSize() { return QSettings().value("random/batchSize", kDefaultRandomBatchSize).toInt(); }
void setRandomBatchSize(int count) { QSettings().setValue("random/batchSize", count); }
QString randomBatchTitlePrefix() {
    return QSettings().value("random/batchTitlePrefix", kDefaultRandomBatchTitlePrefix).toString();
}
void setRandomBatchTitlePrefix(const QString& prefix) { QSettings().setValue("random/batchTitlePrefix", prefix); }
bool randomKeepBackground() { return QSettings().value("random/keepBackground", kDefaultRandomKeepBackground).toBool(); }
void setRandomKeepBackground(bool keep) { QSettings().setValue("random/keepBackground", keep); }
int mutationMinXforms() { return QSettings().value("random/mutationMinXforms", kDefaultMutationMinXforms).toInt(); }
void setMutationMinXforms(int count) { QSettings().setValue("random/mutationMinXforms", count); }
int mutationMaxXforms() { return QSettings().value("random/mutationMaxXforms", kDefaultMutationMaxXforms).toInt(); }
void setMutationMaxXforms(int count) { QSettings().setValue("random/mutationMaxXforms", count); }
int randomSymmetryType() { return QSettings().value("random/symmetryType", kDefaultRandomSymmetryType).toInt(); }
void setRandomSymmetryType(int type) { QSettings().setValue("random/symmetryType", type); }
int randomSymmetryOrder() { return QSettings().value("random/symmetryOrder", kDefaultRandomSymmetryOrder).toInt(); }
void setRandomSymmetryOrder(int order) { QSettings().setValue("random/symmetryOrder", order); }

int randomMinVariationsPerXform() {
    return QSettings().value("random/minVariationsPerXform", kDefaultRandomMinVariationsPerXform).toInt();
}
void setRandomMinVariationsPerXform(int count) { QSettings().setValue("random/minVariationsPerXform", count); }
int randomMaxVariationsPerXform() {
    return QSettings().value("random/maxVariationsPerXform", kDefaultRandomMaxVariationsPerXform).toInt();
}
void setRandomMaxVariationsPerXform(int count) { QSettings().setValue("random/maxVariationsPerXform", count); }
double randomVariationWeightMin() {
    return QSettings().value("random/variationWeightMin", kDefaultRandomVariationWeightMin).toDouble();
}
void setRandomVariationWeightMin(double weight) { QSettings().setValue("random/variationWeightMin", weight); }
double randomVariationWeightMax() {
    return QSettings().value("random/variationWeightMax", kDefaultRandomVariationWeightMax).toDouble();
}
void setRandomVariationWeightMax(double weight) { QSettings().setValue("random/variationWeightMax", weight); }
bool randomizeVariationParameters() {
    return QSettings().value("random/randomizeVariationParameters", kDefaultRandomizeVariationParameters).toBool();
}
void setRandomizeVariationParameters(bool randomize) {
    QSettings().setValue("random/randomizeVariationParameters", randomize);
}
double randomVariationParameterStrength() {
    return QSettings().value("random/variationParameterStrength", kDefaultRandomVariationParameterStrength).toDouble();
}
void setRandomVariationParameterStrength(double strength) {
    QSettings().setValue("random/variationParameterStrength", strength);
}

QStringList disabledVariationNames() { return QSettings().value("random/disabledVariations").toStringList(); }
void setDisabledVariationNames(const QStringList& names) { QSettings().setValue("random/disabledVariations", names); }

std::vector<int> enabledVariationIndices() {
    const QStringList disabled = disabledVariationNames();
    const auto& registry = apo::VariationRegistry::instance();
    std::vector<int> enabled;
    enabled.reserve(static_cast<size_t>(registry.nrVar()));
    for (int i = 0; i < registry.nrVar(); ++i) {
        if (!disabled.contains(QString::fromStdString(registry.varName(i)))) enabled.push_back(i);
    }
    if (enabled.empty()) {
        // Every variation was excluded - fall back to "everything eligible"
        // rather than leaving random generation with nothing to draw from.
        enabled.reserve(static_cast<size_t>(registry.nrVar()));
        for (int i = 0; i < registry.nrVar(); ++i) enabled.push_back(i);
    }
    return enabled;
}

int randomGradientSource() { return QSettings().value("random/gradientSource", kDefaultRandomGradientSource).toInt(); }
void setRandomGradientSource(int source) { QSettings().setValue("random/gradientSource", source); }

QString randomForcedVariationName() {
    return QSettings().value("random/forcedVariationName", kDefaultRandomForcedVariationName).toString();
}
void setRandomForcedVariationName(const QString& name) { QSettings().setValue("random/forcedVariationName", name); }

bool libraryThumbnailView() { return QSettings().value("library/thumbnailView", true).toBool(); }
void setLibraryThumbnailView(bool thumbnails) { QSettings().setValue("library/thumbnailView", thumbnails); }

double defaultGamma() { return QSettings().value("newFlame/gamma", kDefaultDefaultGamma).toDouble(); }
void setDefaultGamma(double gamma) { QSettings().setValue("newFlame/gamma", gamma); }
double defaultBrightness() { return QSettings().value("newFlame/brightness", kDefaultDefaultBrightness).toDouble(); }
void setDefaultBrightness(double brightness) { QSettings().setValue("newFlame/brightness", brightness); }
double defaultVibrancy() { return QSettings().value("newFlame/vibrancy", kDefaultDefaultVibrancy).toDouble(); }
void setDefaultVibrancy(double vibrancy) { QSettings().setValue("newFlame/vibrancy", vibrancy); }
double defaultGammaThreshold() {
    return QSettings().value("newFlame/gammaThreshold", kDefaultDefaultGammaThreshold).toDouble();
}
void setDefaultGammaThreshold(double threshold) { QSettings().setValue("newFlame/gammaThreshold", threshold); }
double defaultSampleDensity() {
    return QSettings().value("newFlame/sampleDensity", kDefaultDefaultSampleDensity).toDouble();
}
void setDefaultSampleDensity(double density) { QSettings().setValue("newFlame/sampleDensity", density); }
int defaultOversample() { return QSettings().value("newFlame/oversample", kDefaultDefaultOversample).toInt(); }
void setDefaultOversample(int oversample) { QSettings().setValue("newFlame/oversample", oversample); }
double defaultFilterRadius() {
    return QSettings().value("newFlame/filterRadius", kDefaultDefaultFilterRadius).toDouble();
}
void setDefaultFilterRadius(double radius) { QSettings().setValue("newFlame/filterRadius", radius); }

} // namespace apo::ui::AppSettings
