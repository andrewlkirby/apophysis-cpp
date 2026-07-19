#pragma once

#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>

#include "../Flame.h"

namespace apo {

// Phase 3: the multithreaded chaos-game renderer. Mirrors the combined
// responsibility of Delphi's TBaseRenderer/TRenderWorkerST/TRenderWorkerMT
// (Rendering/RenderingInterface.pas, RenderingImplementation.pas) plus
// ImageMaker.pas's fixed-filter tone-mapping path - but restructured, not a
// line-for-line port, per the "good performance over faithful copy"
// direction:
//
//   - One render loop, not four TThread-derived classes. Single-threaded
//     and multithreaded rendering share the same per-batch point-generation
//     code (renderBatch() in Renderer.cpp); "single-threaded" is just
//     "multithreaded with threadCount=1", not a separate implementation to
//     keep in sync.
//   - No shared critical section during generation at all. The original
//     serialized every SUB_BATCH_SIZE-point batch through one lock
//     (BucketFillerThread.pas) because every thread wrote into one shared
//     `buckets` array. Since bucket accumulation is a plain per-field `+=`
//     with no ordering dependency (confirmed by tracing every access site -
//     the one thing that would've broken that, the z-buffer depth test
//     behind `{$ifdef ENABLEZBUF}`, is dead code never defined in the
//     actual build), each thread here instead owns a private full-size
//     bucket array and accumulates into it lock-free; buckets are summed
//     once, after every thread joins. Trades peak memory (N private bucket
//     arrays instead of 1 shared one) for zero contention - the right
//     trade for "good performance" on anything but extremely
//     memory-constrained renders.
//   - No intermediate Points[] buffer between generation and accumulation.
//     The original's two-phase design (generate a batch of points, THEN
//     accumulate it) existed specifically to minimize time spent holding
//     the shared lock. With no lock at all, accumulating each point
//     directly as it's generated is simpler and touches less memory.
//   - Adaptive density estimation (variable-width blur kernel, narrower in
//     dense regions and wider in sparse ones, to cut chaos-game noise
//     without over-blurring detail) is a from-scratch design here, not a
//     port of ImageMaker.pas's num_de_filters machinery - that code is
//     provably dead in the shipped original: CreateFilter's only call site
//     for InitDE is `if fcp.enable_de and false then InitDE`, an `and
//     false` that makes the condition permanently unreachable regardless
//     of enable_de's value (verified against source; InitDE itself also
//     carries its own `-X- TODO fix...` comment marking an unfinished
//     branch). So Flame::enableDE having zero effect on the original's
//     actual rendered output isn't a gap this port left open - it's exact
//     behavioral parity with a flag the original also silently ignores.
//     What's implemented here (see buildAdaptiveDE/adaptiveRadiusFor in
//     Renderer.cpp) instead realizes the *intent* the dead code gestured
//     at - map each pixel's local bucket density to a blur radius via
//     estimator (max radius)/estimatorMin (min radius)/estimatorCurve
//     (falloff shape), using precomputed per-integer-radius circular
//     Gaussian kernels - as new, tested, working code, gated behind
//     Flame::enableDE exactly as the field's name always promised.
//   - Fix #11's allocate-buckets-only-when-size-changed optimization (an
//     interactive-preview concern - repeated same-size re-renders while
//     dragging a slider) isn't ported here either: this is a one-shot
//     render() call with no persistent Renderer object to cache an
//     allocation across calls. Revisit alongside Phase 6's interactive
//     preview, where that concern actually applies.
struct RenderStats {
    // Every accepted-into-batch point after the per-batch FUSE warm-up,
    // across every thread - i.e. every point that reached the opacity roll,
    // whether or not it then passed it or landed in-bounds.
    std::uint64_t pointsGenerated = 0;
    // Points that both passed the opacity roll AND landed within camera
    // bounds - i.e. actually contributed to a bucket.
    std::uint64_t pointsAccepted = 0;
};

struct RenderedImage {
    int width = 0;
    int height = 0;
    // Row-major, top-to-bottom, `channels` bytes per pixel (3 = RGB, 4 =
    // RGBA - see Flame::transparency). Empty if the render produced a
    // degenerate (zero-size) image.
    std::vector<std::uint8_t> pixels;
    int channels = 3;
    RenderStats stats;
};

// Optional wall-clock breakdown of one render() call, by phase - B0's
// benchmark harness (src/tools/apo_bench.cpp) is the primary consumer, but
// this is plain diagnostic output any caller can ask for, not a
// benchmark-only concept. Left zero-initialized (all phases 0) if render()
// returns before doing any work (degenerate flame - see render()'s early
// return). "iterate" is the wall-clock span the worker threads were alive
// for (i.e. real parallel wall time, not summed per-thread CPU time) since
// that's what actually gates how long a render takes; "reduce" is the
// single-threaded per-thread-bucket-array summation pass; "tonemap" is the
// final per-pixel filter/gamma/curve pass. setup+iterate+reduce+tonemap
// should sum to very close to the caller's own wall-clock measurement of
// the whole render() call.
struct RenderTimings {
    double setupSeconds = 0;
    double iterateSeconds = 0;
    double reduceSeconds = 0;
    double tonemapSeconds = 0;
};

// FOLLOWUP_PLAN.txt B5. The chaos game's bucket writes are random-access
// over a huge array - the classic flame-fractal cache-miss bottleneck -
// so halving each bucket's size (4 doubles -> 4 floats) halves both the
// working set and the per-thread-copy memory bill (see
// estimatePeakMemoryBytes below). Double is every existing caller's
// unchanged behavior and stays the default everywhere; Float is a new,
// explicitly opt-in tradeoff with two real costs to weigh before choosing
// it: a bucket's `count` saturates (stops incrementing entirely) once it
// accumulates past float's ~16.7M-point mantissa limit - a real concern
// for extreme-density final renders concentrating at attractor hot spots,
// a non-issue for the low-density interactive previews this is actually
// wired into (RenderWorker::renderFlame) - and accumulated color channels
// round differently than double's, changing rendered output (single-
// threaded deterministic output at Float precision differs from Double's -
// same CAVEAT class as B3, except Double stays every existing test/
// baseline's default, so nothing needed regenerating for this item).
enum class BucketPrecision { Double, Float };

// Shared between the render() caller and its in-flight worker threads for
// live progress reporting and cooperative cancellation - the Render
// dialog's progress bar and Cancel button (Phase 6). Deliberately a
// polled atomic pair rather than a callback: render()'s worker threads
// aren't the UI thread, so a callback would need its own marshaling back
// to Qt's event loop anyway - a UI-side QTimer polling these atomics
// (typically every ~100-200ms) is simpler and needs no synchronization
// beyond what's already here.
struct RenderProgress {
    std::atomic<std::uint64_t> pointsDone{0};
    std::atomic<std::uint64_t> pointsTarget{0};
    // Checked by every worker thread after each ~10,000-point sub-batch
    // (kSubBatchSize) - so a cancellation request is honored within one
    // sub-batch's worth of work, not instantaneously but fast enough for
    // an interactive Cancel button. A cancelled render still tone-maps and
    // returns whatever was accumulated into the buckets so far (a noisier
    // but valid partial image), rather than throwing the work away.
    std::atomic<bool> cancelRequested{false};

    // Checked at the same per-sub-batch points as cancelRequested above.
    // Unlike cancelRequested, this doesn't stop the render - it blocks
    // whichever thread observes it (a short sleep-and-recheck loop, not a
    // busy-spin) until either cleared again or cancelRequested is set,
    // *without* tearing down that thread's private bucket array or
    // ThreadState (see Renderer.cpp's render(), which owns those for the
    // entire call). Chaos-game accumulation has no notion of a "step" to
    // resume from - clearing this just lets the same in-flight render()
    // call keep adding points to the same buckets it already had, so a
    // pause/resume cycle costs nothing (no restart, no lost progress),
    // unlike cancelRequested which ends the render() call outright. Setting
    // both at once resolves to cancel: a render blocked here checks
    // cancelRequested too, so a paused render can still be cancelled
    // without needing to resume it first.
    std::atomic<bool> pauseRequested{false};
};

class Renderer {
public:
    // Renders `flame` to a tone-mapped RGB(A) image.
    //
    // threadCount: 0 (default) uses std::thread::hardware_concurrency();
    // 1 forces the single-threaded path, which is fully deterministic -
    // the same flame + seed always produces a bit-identical image, useful
    // as a test/regression baseline (matches the original's own
    // TRenderWorkerST being the sole bit-reproducible path per
    // OPTIMIZATION_STATUS.md Fix #1); >1 spins up that many worker
    // threads, each with its own RNG stream, so output is NOT
    // bit-reproducible run-to-run even with the same seed - an accepted
    // tradeoff carried forward unchanged from the original (see Rng.h).
    //
    // progress: optional (nullptr by default - no overhead for callers
    // that don't need it, e.g. every existing preview-render call site).
    // When given, pointsTarget is set once near the start of the render
    // and pointsDone is updated as work completes; the caller may set
    // cancelRequested at any time from any thread to stop the render
    // early.
    // timings: optional (nullptr by default, same zero-overhead-when-unused
    // convention as `progress`) - see RenderTimings above.
    // precision: BucketPrecision::Double by default - see its own doc
    // comment above for the Float tradeoff this opts into.
    static RenderedImage render(const Flame& flame, std::uint64_t seed, int threadCount = 0,
                                 RenderProgress* progress = nullptr, RenderTimings* timings = nullptr,
                                 BucketPrecision precision = BucketPrecision::Double);

    // Estimated peak resident memory (bytes) a render() call with these
    // exact flame settings/threadCount/precision would use - dominated by
    // (threadCount+1) private/reduction bucket arrays (see this class's
    // own comment above on the lock-free-private-bucket-per-thread
    // design), each bucketWidth*bucketHeight*(16 or 32 bytes depending on
    // `precision`); every other scratch allocation is comparatively
    // negligible and omitted. Pure arithmetic derived from flame.width/
    // height/spatialOversample/spatialFilterRadius (the same sizing
    // formulas render() itself uses - see Renderer.cpp's buildFilter/
    // buildBuffersAndCamera) - runs no chaos-game iteration at all, so
    // it's cheap enough to call on every UI field edit (RenderDialog's
    // memory-usage estimate line). Same threadCount=0-means-
    // hardware_concurrency() convention as render().
    static std::uint64_t estimatePeakMemoryBytes(const Flame& flame, int threadCount = 0,
                                                  BucketPrecision precision = BucketPrecision::Double);

    // A cheap chaos-game position sample: runs the same weighted xform
    // selection and FUSE warm-up render() itself uses, but tracks only raw,
    // pre-camera-projection (x,y) positions - no color, no bucket
    // accumulation, no tone-mapping. Matches ControlPoint.pas's IterateXY
    // in purpose and camera-agnosticism (see AutoFrame.h, this sample's
    // only current caller, for why not projecting matters: framing needs
    // to know where the *unprojected* attractor sits). Always single-
    // threaded and deterministic given a seed - a one-off diagnostic
    // sample, far cheaper than a real render, not something worth
    // parallelizing. Returns fewer than `count` points if the flame is
    // degenerate (no active xforms) or the chaos game hits a non-finite
    // point partway through (matches render()'s own bail-out-the-rest-of-
    // this-batch behavior for the same situation).
    static std::vector<std::pair<double, double>> samplePoints(const Flame& flame, std::uint64_t seed, int count);
};

} // namespace apo
