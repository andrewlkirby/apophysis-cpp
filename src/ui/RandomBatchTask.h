#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include <QObject>
#include <QRunnable>
#include <QString>

#include "core/Flame.h"
#include "core/edit/RandomFlame.h"
#include "core/render/AutoFrame.h"

namespace apo::ui {

// Everything a RandomBatchTask needs to know that's shared across an entire
// "New Random Batch" run (as opposed to per-slot state like the slot index
// or its seed) - built once, on the GUI thread, in
// MainWindow::generateRandomBatch, then handed to every task via a single
// shared_ptr<const RandomBatchSettings> rather than copied per-task (a
// large batch can have hundreds of concurrently-queued tasks, and
// eligibleVariations/currentGradient aren't free to duplicate that many
// times over).
//
// Deliberately a plain snapshot of AppSettings' current values, not a
// pointer back to AppSettings itself: AppSettings/QSettings reads happen
// exactly once, up front, on the GUI thread, so no RandomBatchTask ever
// touches QSettings from a worker thread.
struct RandomBatchSettings {
    int width = 0;
    int height = 0;
    int minXforms = apo::kDefaultMinRandomXforms;
    int maxXforms = apo::kDefaultMaxRandomXforms;
    int forcedVariationIndex = -1;
    std::vector<int> eligibleVariations;
    apo::RandomGradientSource gradientSource = apo::RandomGradientSource::Calculated;
    bool hasCurrentGradient = false;
    apo::ColorMap currentGradient;
    int minVariationsPerXform = 1;
    int maxVariationsPerXform = 1;
    double variationWeightMin = 1.0;
    double variationWeightMax = 1.0;
    bool randomizeVariationParameters = false;
    double variationParameterStrength = 1.0;
    // 0=None, 1=Bilateral, 2=Rotational, 3=Dihedral - matches Options.dfm's
    // cmbSymType.Items order (see AppSettings.h).
    int symType = 0;
    int symOrder = 1;
    bool discardBlank = false;
    int minFramingSamples = apo::kDefaultMinValidSamples;
    double minColoredCoverage = apo::kDefaultMinColoredCoverage;
    int coverageTestSize = apo::kDefaultCoverageTestSize;
    bool keepBackground = false;
    std::array<int, 4> keepBackgroundColor{0, 0, 0, 0};
    QString titlePrefix;

    // Options.pas's Display tab "Rendering" group (AppSettings::defaultXxx),
    // snapshotted here for the same reason as everything else in this
    // struct - see applyRenderDefaults() below, the per-task replacement for
    // MainWindow.cpp's own applyDefaultRenderSettings() free function (which
    // reads AppSettings directly, so stays GUI-thread-only and unused here).
    double gamma = 1.0;
    double brightness = 1.0;
    double vibrancy = 1.0;
    double gammaThreshold = 0.01;
    double sampleDensity = 1.0;
    int spatialOversample = 1;
    double spatialFilterRadius = 0.0;

    void applyRenderDefaults(apo::Flame& flame) const {
        flame.gamma = gamma;
        flame.brightness = brightness;
        flame.vibrancy = vibrancy;
        flame.gammaThreshold = gammaThreshold;
        flame.sampleDensity = sampleDensity;
        flame.spatialOversample = spatialOversample;
        flame.spatialFilterRadius = spatialFilterRadius;
    }
};

// One batch slot's worth of "New Random Batch" work - generate, force
// symmetry, and (while discardBlank is set) retry against the quality
// filters up to kMaxDiscardRetries times - run on
// QThreadPool::globalInstance(). The multi-slot counterpart to
// ThumbnailTask.h's single-thumbnail runnable, and built the same way
// (QRunnable+QObject, setAutoDelete(true), a `finished` signal Qt marshals
// back to the GUI thread automatically since it's emitted from a different
// thread than the receiver lives on - see ThumbnailTask.h's own comment for
// why that's safe without any explicit queuing on this class's part).
//
// Every slot is fully independent: it's seeded from `baseSeed`/`index` only
// (the exact same derivation MainWindow::generateRandomBatch always used,
// back when this loop ran sequentially), so which slot happens to finish
// first now that they run concurrently doesn't change what any slot ends up
// containing - only how many run at once, and in what order `finished`
// fires.
class RandomBatchTask final : public QObject, public QRunnable {
    Q_OBJECT

public:
    // Matches MainWindow.cpp's old in-loop constant exactly - kept here
    // (not in RandomBatchSettings) since it's not a user-facing setting,
    // just this task's own retry budget.
    static constexpr int kMaxDiscardRetries = 20;

    RandomBatchTask(int index, std::uint64_t baseSeed, std::shared_ptr<const RandomBatchSettings> settings,
                     std::shared_ptr<std::atomic<bool>> cancelled)
        : index_(index), baseSeed_(baseSeed), settings_(std::move(settings)), cancelled_(std::move(cancelled)) {
        setAutoDelete(true);
    }

    void run() override;

signals:
    // `wasFallback` mirrors generateRandomBatch's own fallbackCount
    // bookkeeping: true when every discard-retry attempt failed the quality
    // filters and the best-scoring attempt was kept instead of one that
    // actually passed.
    void finished(int index, std::shared_ptr<apo::Flame> flame, bool wasFallback);

private:
    int index_;
    std::uint64_t baseSeed_;
    std::shared_ptr<const RandomBatchSettings> settings_;
    // Polled between attempts (never before the first - see run()'s own
    // comment) so a cancelled batch stops burning through retries almost
    // immediately without needing every in-flight task to finish its full
    // kMaxDiscardRetries budget first.
    std::shared_ptr<std::atomic<bool>> cancelled_;
};

} // namespace apo::ui
