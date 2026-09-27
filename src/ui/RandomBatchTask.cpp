#include "RandomBatchTask.h"

#include "core/edit/Symmetry.h"

namespace apo::ui {

void RandomBatchTask::run() {
    const RandomBatchSettings& s = *settings_;

    std::unique_ptr<apo::Flame> flame;
    // Best-scoring attempt seen so far this slot, kept in case every attempt
    // fails discardBlank's checks (or cancellation cuts the retry loop
    // short) - see the fallback below. Same shape as
    // MainWindow::generateRandomBatch's old sequential loop.
    std::unique_ptr<apo::Flame> bestFlame;
    double bestScore = -1.0;
    bool accepted = false;

    for (int attempt = 0; attempt < kMaxDiscardRetries; ++attempt) {
        // Never bail before the first attempt: that would leave `flame`
        // null, and this always needs *something* valid to emit. Checked
        // only between attempts, so a cancelled batch stops retrying almost
        // immediately without ever completing a full kMaxDiscardRetries
        // budget it no longer needs.
        if (attempt > 0 && cancelled_->load(std::memory_order_relaxed)) break;

        const std::uint64_t seed =
            baseSeed_ + (static_cast<std::uint64_t>(index_) * static_cast<std::uint64_t>(kMaxDiscardRetries) +
                         static_cast<std::uint64_t>(attempt)) *
                            0x9e3779b97f4a7c15ULL;
        bool hasContent = true;
        flame = apo::generateRandomFlame(seed, s.width, s.height, s.minXforms, s.maxXforms, s.forcedVariationIndex,
                                          &s.eligibleVariations, s.gradientSource,
                                          s.hasCurrentGradient ? &s.currentGradient : nullptr, s.minVariationsPerXform,
                                          s.maxVariationsPerXform, s.variationWeightMin, s.variationWeightMax,
                                          s.randomizeVariationParameters, s.variationParameterStrength, &hasContent,
                                          s.minFramingSamples);

        // Forced symmetry on random generation (Options.pas's Random tab) -
        // matches Main.pas's own sign convention exactly (see
        // core/edit/Symmetry.h's doc comment): Bilateral -> sym=-1,
        // Rotational -> sym=+Order, Dihedral -> sym=-Order.
        switch (s.symType) {
            case 1: apo::addSymmetry(*flame, -1); break;
            case 2: apo::addSymmetry(*flame, s.symOrder); break;
            case 3: apo::addSymmetry(*flame, -s.symOrder); break;
            default: break; // 0 = None
        }

        // generateRandomFlame() already auto-framed the flame, but that
        // framing was computed before addSymmetry() above ran - see
        // MainWindow.cpp's old comment (now here) for why re-framing against
        // the fully-built (post-symmetry) flame matters. Its return value
        // supersedes the pre-symmetry hasContent for the discard check below.
        if (s.symType != 0) hasContent = apo::autoFrameFlame(*flame, seed + 7, s.minFramingSamples);

        if (!s.discardBlank) { accepted = true; break; }

        // Apply the render settings (and "keep background") this flame will
        // actually be kept with *before* measuring its content below - see
        // RandomBatchSettings::applyRenderDefaults's own doc comment for why
        // (scoring generateRandomFlame's own gamma=1/brightness=1 defaults
        // would silently judge a much darker image than what's actually kept).
        s.applyRenderDefaults(*flame);
        if (s.keepBackground) flame->background = s.keepBackgroundColor;

        // A flame can pass the spatial framing check above (a real,
        // non-degenerate attractor extent) and still render as barely more
        // than a faint dot/smudge - see AutoFrame.h's
        // hasMinimumColoredCoverage doc comment for why. Only worth the
        // extra test render when framing already succeeded - otherwise this
        // attempt is getting discarded regardless of coverage.
        double score = 0.0;
        if (hasContent) {
            const apo::ContentScore content = apo::measureContent(*flame, seed + 13, s.coverageTestSize);
            score = content.score;
            hasContent = content.coveredFraction >= s.minColoredCoverage;
        }

        if (hasContent) { accepted = true; break; }

        // Degenerate, or real but below the coverage bar - remember this
        // attempt if it's the best-scoring one seen so far for this slot,
        // then loop and retry with a new seed. If every attempt fails (or
        // cancellation cuts this short), the best-scoring attempt is kept
        // below, so neither a strict filter nor a cancel can leave this slot
        // with nothing.
        if (!bestFlame || score > bestScore) {
            bestScore = score;
            bestFlame = std::move(flame);
        }
    }

    bool wasFallback = false;
    if (!accepted) {
        if (bestFlame) flame = std::move(bestFlame);
        wasFallback = true;
    }

    flame->name = QString("%1 %2").arg(s.titlePrefix).arg(index_ + 1).toStdString();
    // When discardBlank is on, every attempt above (accepted or not) already
    // had render settings/background applied before its content was
    // measured - applying again here would be redundant. When it's off,
    // nothing above touched them, so apply now.
    if (!s.discardBlank) {
        s.applyRenderDefaults(*flame);
        if (s.keepBackground) flame->background = s.keepBackgroundColor;
    }

    emit finished(index_, std::shared_ptr<apo::Flame>(std::move(flame)), wasFallback);
}

} // namespace apo::ui
