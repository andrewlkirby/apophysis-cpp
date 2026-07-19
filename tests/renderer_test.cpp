// Tests for Phase 3's Renderer (src/core/render/Renderer.h). There's no
// Phase 4 file I/O yet, so these hand-build a simple flame in code (a
// classic 3-map Sierpinski-gasket IFS: three half-scale affine xforms
// toward the corners of a triangle) rather than loading a .flame file.

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/io/ImageCompare.h"
#include "core/render/Renderer.h"

using apo_test::check;

namespace {

// A grayscale ramp - not visually interesting, but deterministic and
// trivial to reason about (bucket color output should track sample
// density directly, with no colormap-shape confound).
void setGrayscaleColorMap(apo::Flame& flame) {
    for (int i = 0; i < 256; ++i) {
        flame.cmap.entries[i][0] = i;
        flame.cmap.entries[i][1] = i;
        flame.cmap.entries[i][2] = i;
        flame.cmap.entries[i][3] = 255;
    }
}

std::unique_ptr<apo::Flame> makeSierpinskiFlame(int size = 64) {
    auto flame = std::make_unique<apo::Flame>();
    flame->width = size;
    flame->height = size;
    flame->spatialOversample = 1;
    flame->sampleDensity = 20;
    flame->center = {0.0, 0.0};
    flame->pixelsPerUnit = size / 2.0; // fit the unit-ish triangle in-frame
    flame->whiteLevel = 200;
    setGrayscaleColorMap(*flame);

    // Three half-scale affine maps toward (-0.5,-0.5), (0.5,-0.5), (0,0.5) -
    // the standard Sierpinski-gasket IFS. All use the default "linear"
    // variation (weight 1, everything else 0 - XForm::clear()'s default).
    const double corners[3][2] = {{-0.5, -0.5}, {0.5, -0.5}, {0.0, 0.5}};
    for (int i = 0; i < 3; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.color = i / 2.0; // 0, 0.5, 1 - spread across the colormap
        xf.c[0] = {0.5, 0.0};
        xf.c[1] = {0.0, 0.5};
        xf.c[2] = {corners[i][0], corners[i][1]};
    }
    return flame;
}

void testSingleThreadedIsDeterministic() {
    auto flame = makeSierpinskiFlame();
    apo::RenderedImage a = apo::Renderer::render(*flame, /*seed=*/42, /*threadCount=*/1);
    apo::RenderedImage b = apo::Renderer::render(*flame, /*seed=*/42, /*threadCount=*/1);

    check(a.width == flame->width && a.height == flame->height, "rendered image has the requested dimensions");
    check(a.pixels.size() == b.pixels.size() && a.pixels == b.pixels,
          "single-threaded render is bit-reproducible for the same flame+seed");
    check(a.stats.pointsAccepted > 0, "single-threaded render accepted at least some points");
}

void testDifferentSeedsProduceDifferentOutput() {
    auto flame = makeSierpinskiFlame();
    apo::RenderedImage a = apo::Renderer::render(*flame, /*seed=*/1, /*threadCount=*/1);
    apo::RenderedImage b = apo::Renderer::render(*flame, /*seed=*/2, /*threadCount=*/1);
    check(a.pixels != b.pixels, "different seeds produce different pixel output (same flame, single-threaded)");
}

void testMultithreadedRenderCompletesAndCoversCanvas() {
    auto flame = makeSierpinskiFlame();
    // sampleDensity chosen so even a small render clearly covers a good
    // fraction of the canvas - the Sierpinski attractor fills a large
    // portion of its bounding triangle at this depth.
    apo::RenderedImage img = apo::Renderer::render(*flame, /*seed=*/7, /*threadCount=*/4);

    check(img.width == flame->width && img.height == flame->height, "multithreaded render has correct dimensions");
    check(!img.pixels.empty(), "multithreaded render produced pixel data");
    check(img.stats.pointsAccepted > 0, "multithreaded render accepted points across its worker threads");

    int nonBackgroundPixels = 0;
    const int pixelCount = img.width * img.height;
    for (int p = 0; p < pixelCount; ++p) {
        const size_t off = static_cast<size_t>(p) * img.channels;
        if (img.pixels[off] != 0 || img.pixels[off + 1] != 0 || img.pixels[off + 2] != 0) {
            ++nonBackgroundPixels;
        }
    }
    // Loose bound - not asserting an exact coverage percentage (that would
    // make this test fragile to filter/tone-map tuning), just that a
    // meaningful fraction of the canvas actually got drawn on, not "1
    // stray pixel" or "completely blank".
    check(nonBackgroundPixels > pixelCount / 10,
          "a meaningful fraction of the canvas has non-background pixels");
}

void testPartialOpacityStillRejectsPointsStochastically() {
    // FOLLOWUP_PLAN.txt B3 fast-paths transOpacity==1 (skips the RNG draw
    // entirely) but must leave transOpacity<1's real stochastic roll
    // exactly as it was - this flame's xforms all have transOpacity=0.5,
    // so roughly half of every generated point should still be rejected by
    // the roll (rng.uniform01() >= 0.5), same as before this item existed.
    auto flame = makeSierpinskiFlame();
    for (int i = 0; i < flame->numXForms(); ++i) flame->xform[i]->transOpacity = 0.5;

    apo::RenderedImage img = apo::Renderer::render(*flame, /*seed=*/21, /*threadCount=*/1);

    check(img.stats.pointsGenerated > 0, "the render still generates points normally");
    check(img.stats.pointsAccepted > 0, "some points still get accepted despite partial opacity");
    check(img.stats.pointsAccepted < img.stats.pointsGenerated,
          "transOpacity=0.5 still rejects a real fraction of points - the opacity roll isn't skipped for "
          "anything but exactly transOpacity==1");
    const double acceptFraction =
        static_cast<double>(img.stats.pointsAccepted) / static_cast<double>(img.stats.pointsGenerated);
    check(acceptFraction > 0.3 && acceptFraction < 0.7,
          "the accepted fraction is in the right ballpark for a ~50% opacity roll (loose bound, not an exact "
          "statistical claim)");
}

void testFullOpacityAcceptsEveryFinitePoint() {
    // The complementary case: transOpacity==1 (every xform's default) must
    // still accept every point that isn't itself non-finite/out-of-bounds -
    // the fast path removes the RNG draw, not the accept outcome.
    auto flame = makeSierpinskiFlame();
    apo::RenderedImage img = apo::Renderer::render(*flame, /*seed=*/21, /*threadCount=*/1);

    check(img.stats.pointsGenerated > 0, "the render generates points normally");
    check(img.stats.pointsAccepted == img.stats.pointsGenerated,
          "with every xform at transOpacity==1 (the default), every generated point is accepted - a Sierpinski "
          "IFS's affine maps never produce a non-finite or off-canvas point");
}

void testMultithreadedRenderWithMoreThreadsThanImageRows() {
    // B4 parallelizes bucket-reduction and tone-mapping across the render's
    // own worker-thread count via a static contiguous split (parallelFor in
    // Renderer.cpp) - every other multithreaded test in this file uses
    // images tall/dense enough that totalWork (image rows for tone-mapping,
    // bucket count for reduction) always comfortably exceeds threadCount,
    // so none of them exercise parallelFor's "more threads requested than
    // units of work" clamp, or a row count that doesn't divide evenly by
    // the thread count (a classic place for an off-by-one in chunk-boundary
    // arithmetic to hide). height=7 with threadCount=16 forces both at
    // once for the tone-map pass (7 rows, not a multiple of 16, split
    // across more threads than there are rows).
    auto flame = makeSierpinskiFlame(64);
    flame->height = 7;
    flame->sampleDensity = 300;

    apo::RenderedImage img = apo::Renderer::render(*flame, /*seed=*/13, /*threadCount=*/16);

    check(img.width == 64 && img.height == 7,
          "a render with more worker threads than image rows still produces the requested dimensions");
    check(img.pixels.size() == static_cast<size_t>(64 * 7 * img.channels),
          "every row's pixel data is present - none skipped or left uninitialized by the clamp");
    check(img.stats.pointsAccepted > 0, "the render still accepts points normally");

    // Same loose-coverage idiom as testMultithreadedRenderCompletesAndCoversCanvas
    // above (not a per-row guarantee, which would be fragile against the
    // attractor's own shape) - if a chunk-boundary bug dropped or duplicated
    // a range of rows, coverage would collapse well below this bound.
    int nonBackgroundPixels = 0;
    const int pixelCount = img.width * img.height;
    for (int p = 0; p < pixelCount; ++p) {
        const size_t off = static_cast<size_t>(p) * img.channels;
        if (img.pixels[off] != 0 || img.pixels[off + 1] != 0 || img.pixels[off + 2] != 0) ++nonBackgroundPixels;
    }
    check(nonBackgroundPixels > pixelCount / 10,
          "a meaningful fraction of the thin canvas still got real tone-mapped output, not just a lucky first chunk");
}

void testDegenerateFlameProducesEmptyImage() {
    apo::Flame flame; // fresh, unconfigured: every xform has density=0
    apo::RenderedImage img = apo::Renderer::render(flame, /*seed=*/1, /*threadCount=*/1);
    check(img.pixels.empty() && img.width == 0 && img.height == 0,
          "a flame with no active xforms renders to an empty image instead of crashing");
}

void testFinalXformIsApplied() {
    // A final xform that unconditionally translates every point far outside
    // the camera's view should push the entire image to background-only,
    // confirming useFinalXform actually routes through Renderer (and not
    // just that regular xforms work).
    auto flame = makeSierpinskiFlame();
    flame->finalXformEnabled = true;
    flame->useFinalXform = true;
    apo::XForm& fx = *flame->finalXform;
    fx.clear();
    fx.c[2] = {1000.0, 1000.0}; // translate every point far off-canvas

    apo::RenderedImage img = apo::Renderer::render(*flame, /*seed=*/3, /*threadCount=*/1);
    int nonBackgroundPixels = 0;
    for (size_t p = 0; p < img.pixels.size() / img.channels; ++p) {
        const size_t off = p * img.channels;
        if (img.pixels[off] != 0 || img.pixels[off + 1] != 0 || img.pixels[off + 2] != 0) {
            ++nonBackgroundPixels;
        }
    }
    check(nonBackgroundPixels == 0, "an off-canvas-translating final xform blanks the whole image");
}

void testDefaultCurvesDoNotChangeOutput() {
    // Every Flame defaults to the identity curve (see Flame.h's `curves`
    // field) - baseline_regression_test.cpp already confirms this
    // end-to-end (bit-identical PSNR=inf against pre-curves-feature
    // baselines), this is the same claim as a focused unit test.
    auto flame = makeSierpinskiFlame();
    apo::RenderedImage withDefaultCurves = apo::Renderer::render(*flame, /*seed=*/9, /*threadCount=*/1);
    apo::RenderedImage again = apo::Renderer::render(*flame, /*seed=*/9, /*threadCount=*/1);
    check(withDefaultCurves.pixels == again.pixels, "identical default-curve renders are bit-reproducible");
}

void testNonDefaultMasterCurveChangesOutput() {
    auto flameA = makeSierpinskiFlame();
    auto flameB = makeSierpinskiFlame();
    // Pull the master curve's midtones down toward black - a "darken"
    // curve, distinct enough from the y=x default to be unmissable in the
    // rendered output.
    flameB->curves[0].points[1] = {0.0, 0.0};
    flameB->curves[0].points[2] = {1.0, 0.2};

    const apo::RenderedImage imgA = apo::Renderer::render(*flameA, /*seed=*/9, /*threadCount=*/1);
    const apo::RenderedImage imgB = apo::Renderer::render(*flameB, /*seed=*/9, /*threadCount=*/1);

    check(imgA.pixels != imgB.pixels, "a non-default master curve produces different pixel output than the default");

    long sumA = 0, sumB = 0;
    for (auto v : imgA.pixels) sumA += v;
    for (auto v : imgB.pixels) sumB += v;
    check(sumB < sumA, "a darkening master curve actually darkens the rendered image, not just changes it arbitrarily");
}

void testCurvesSetOnlyChecksPointsNotWeights() {
    // Matches ImageMaker.pas's curvesSet detection exactly (see
    // Renderer.cpp's buildCurveTables doc comment): a curve edited via
    // weight alone, with every point left at the exact default identity
    // position, is invisible at render time in this port, same as the
    // original - not a bug this port introduces.
    auto flameA = makeSierpinskiFlame();
    auto flameB = makeSierpinskiFlame();
    flameB->curves[1].weights = {1.0, 5.0, 5.0, 1.0}; // weight-only edit, points untouched

    const apo::RenderedImage imgA = apo::Renderer::render(*flameA, /*seed=*/9, /*threadCount=*/1);
    const apo::RenderedImage imgB = apo::Renderer::render(*flameB, /*seed=*/9, /*threadCount=*/1);
    check(imgA.pixels == imgB.pixels, "a weight-only curve edit (points still at the default) has no render effect");
}

void testAdaptiveDEOffByDefaultEvenWithPositiveEstimator() {
    // Flame::estimator defaults to 9.0 (a real, positive value) - only
    // enableDE itself gates whether adaptive DE actually runs.
    auto flameA = makeSierpinskiFlame();
    auto flameB = makeSierpinskiFlame();
    flameB->enableDE = true;

    const apo::RenderedImage imgA = apo::Renderer::render(*flameA, /*seed=*/5, /*threadCount=*/1);
    const apo::RenderedImage imgB = apo::Renderer::render(*flameB, /*seed=*/5, /*threadCount=*/1);
    check(imgA.pixels != imgB.pixels,
          "enabling adaptive DE changes rendered output even though every flame defaults to a positive estimator");
}

void testAdaptiveDERequiresPositiveEstimator() {
    auto flameA = makeSierpinskiFlame();
    flameA->enableDE = true;
    flameA->estimator = 0.0; // explicitly disabled via a non-positive estimator

    auto flameB = makeSierpinskiFlame();
    flameB->enableDE = false;

    const apo::RenderedImage imgA = apo::Renderer::render(*flameA, /*seed=*/5, /*threadCount=*/1);
    const apo::RenderedImage imgB = apo::Renderer::render(*flameB, /*seed=*/5, /*threadCount=*/1);
    check(imgA.pixels == imgB.pixels,
          "enableDE=true with a non-positive estimator behaves identically to enableDE=false (matches "
          "buildAdaptiveDE's own e<=0 guard, ported from InitDE's documented intent)");
}

void testAdaptiveDESmoothsNoiseInASparseLowDensityRender() {
    // A deliberately sparse render (low sample density, small canvas) so
    // the Sierpinski attractor's bucket counts vary a lot pixel-to-pixel -
    // exactly the noisy regime adaptive DE exists to smooth. Same seed for
    // both renders, so the only difference is DE on/off.
    auto flameA = makeSierpinskiFlame(96);
    flameA->sampleDensity = 3;
    auto flameB = makeSierpinskiFlame(96);
    flameB->sampleDensity = 3;
    flameB->enableDE = true;

    const apo::RenderedImage imgA = apo::Renderer::render(*flameA, /*seed=*/11, /*threadCount=*/1);
    const apo::RenderedImage imgB = apo::Renderer::render(*flameB, /*seed=*/11, /*threadCount=*/1);

    // Local pixel-to-pixel noise proxy: mean absolute difference between
    // horizontally-adjacent pixels' red channel, over every non-background
    // pixel - a smoother (less noisy) image has a lower value here.
    auto localVariation = [](const apo::RenderedImage& img) {
        double total = 0;
        int count = 0;
        for (int y = 0; y < img.height; ++y) {
            for (int x = 0; x + 1 < img.width; ++x) {
                const size_t off = (static_cast<size_t>(y) * img.width + x) * img.channels;
                const size_t offNext = off + img.channels;
                if (img.pixels[off] == 0 && img.pixels[off + 1] == 0 && img.pixels[off + 2] == 0) continue;
                total += std::abs(static_cast<int>(img.pixels[off]) - static_cast<int>(img.pixels[offNext]));
                ++count;
            }
        }
        return count > 0 ? total / count : 0.0;
    };

    const double noiseA = localVariation(imgA);
    const double noiseB = localVariation(imgB);
    const std::string msg = "adaptive DE measurably reduces pixel-to-pixel noise in a sparse, low-sample-density "
                             "render (mean adjacent-pixel difference: DE off = " +
                             std::to_string(noiseA) + ", DE on = " + std::to_string(noiseB) + ")";
    check(noiseB < noiseA, msg.c_str());
}

void testAdaptiveDEWithHugeEstimatorStaysBoundedAndFinishes() {
    // A pathologically large estimator should get clamped to the buffer's
    // actual gutter width (buildAdaptiveDE's safety cap), not attempt an
    // unbounded kernel or read out of the bucket array - this just needs
    // to complete without crashing/hanging and produce a well-formed image.
    auto flame = makeSierpinskiFlame(32);
    flame->enableDE = true;
    flame->estimator = 1000.0;

    const apo::RenderedImage img = apo::Renderer::render(*flame, /*seed=*/3, /*threadCount=*/1);
    check(img.width == 32 && img.height == 32 && !img.pixels.empty(),
          "an extreme estimator value still produces a well-formed image (radius clamped to the gutter, not "
          "unbounded)");
}

void testDefaultPrecisionIsDoubleAndBitReproducible() {
    // The default BucketPrecision::Double path must stay byte-for-byte what
    // it always was (FOLLOWUP_PLAN.txt B5 explicitly must not change any
    // existing caller's behavior) - a render() call omitting `precision`
    // altogether must produce the exact same bytes as one explicitly
    // passing BucketPrecision::Double.
    auto flame = makeSierpinskiFlame();
    apo::RenderedImage implicit = apo::Renderer::render(*flame, /*seed=*/5, /*threadCount=*/1);
    apo::RenderedImage explicitDouble =
        apo::Renderer::render(*flame, /*seed=*/5, /*threadCount=*/1, nullptr, nullptr, apo::BucketPrecision::Double);
    check(implicit.pixels == explicitDouble.pixels,
          "omitting BucketPrecision defaults to exactly BucketPrecision::Double, bit-for-bit");
}

void testFloatBucketsProduceAValidSimilarImage() {
    // Float buckets are a lower-precision accumulation, not a different
    // algorithm - a float-precision render of the same flame/seed should
    // still be a well-formed image, still generate/accept the same number
    // of points (precision only affects tone-mapped color accumulation, not
    // point generation/acceptance at all), and should still closely resemble
    // (not exactly equal - see this test's own PSNR check) the double-
    // precision render of the identical flame+seed.
    // High enough density that per-bucket accumulated sums have enough
    // significant digits for float's coarser precision to actually round
    // differently than double's by the time tone-mapping quantizes to 8
    // bits - at low density the two precisions can legitimately land on
    // bit-identical output (float has plenty of precision for a handful of
    // points per bucket), which would make this test's own "genuinely
    // different bytes" check below meaningless. Average points-per-bucket
    // tracks sampleDensity directly, independent of canvas size, so a small
    // canvas keeps this test fast without needing a lower density.
    auto flame = makeSierpinskiFlame(40);
    flame->sampleDensity = 4000;

    apo::RenderedImage doubleImg =
        apo::Renderer::render(*flame, /*seed=*/11, /*threadCount=*/1, nullptr, nullptr, apo::BucketPrecision::Double);
    apo::RenderedImage floatImg =
        apo::Renderer::render(*flame, /*seed=*/11, /*threadCount=*/1, nullptr, nullptr, apo::BucketPrecision::Float);

    check(floatImg.width == doubleImg.width && floatImg.height == doubleImg.height &&
              floatImg.channels == doubleImg.channels,
          "a float-precision render has identical dimensions/channels to the double-precision render");
    check(!floatImg.pixels.empty(), "a float-precision render still produces real pixel data");
    check(floatImg.stats.pointsGenerated == doubleImg.stats.pointsGenerated &&
              floatImg.stats.pointsAccepted == doubleImg.stats.pointsAccepted,
          "bucket precision affects color accumulation only - point generation/acceptance counts are identical "
          "between the two precisions for the same flame+seed");
    check(floatImg.pixels != doubleImg.pixels,
          "float and double precision genuinely produce different bytes - the precision argument isn't silently "
          "ignored");

    const double psnr = apo::computePsnr(floatImg.width, floatImg.height, floatImg.channels, floatImg.pixels.data(),
                                          doubleImg.pixels.data());
    char msg[160];
    std::snprintf(msg, sizeof(msg), "float-precision output still closely resembles double-precision output "
                                     "(PSNR=%.1f dB) - lower precision, not a different image",
                  psnr);
    check(psnr > 30.0, msg);
}

void testFloatPrecisionHalvesTheMemoryEstimate() {
    auto flame = makeSierpinskiFlame(256);
    const std::uint64_t doubleBytes =
        apo::Renderer::estimatePeakMemoryBytes(*flame, /*threadCount=*/4, apo::BucketPrecision::Double);
    const std::uint64_t floatBytes =
        apo::Renderer::estimatePeakMemoryBytes(*flame, /*threadCount=*/4, apo::BucketPrecision::Float);
    check(floatBytes * 2 == doubleBytes,
          "Float buckets (16 bytes each) are estimated at exactly half of Double buckets (32 bytes each)");
}

void testEstimatePeakMemoryDefaultsToDouble() {
    auto flame = makeSierpinskiFlame(256);
    const std::uint64_t implicit = apo::Renderer::estimatePeakMemoryBytes(*flame, /*threadCount=*/4);
    const std::uint64_t explicitDouble =
        apo::Renderer::estimatePeakMemoryBytes(*flame, /*threadCount=*/4, apo::BucketPrecision::Double);
    check(implicit == explicitDouble, "omitting BucketPrecision estimates exactly as BucketPrecision::Double would");
}

void testProgressReportsTargetAndReachesCompletion() {
    // A larger flame so there are enough sub-batches (target/10000) for
    // intermediate progress to be observable rather than jumping straight
    // to 100% in one batch.
    auto flame = makeSierpinskiFlame(128);
    flame->sampleDensity = 200;

    apo::RenderProgress progress;
    check(progress.pointsTarget.load() == 0, "pointsTarget starts at 0 before render() is called");

    apo::RenderedImage img = apo::Renderer::render(*flame, /*seed=*/7, /*threadCount=*/1, &progress);

    check(progress.pointsTarget.load() > 0, "render() sets a nonzero pointsTarget for a real flame");
    check(progress.pointsDone.load() == progress.pointsTarget.load(),
          "an uncancelled render's pointsDone reaches exactly pointsTarget by completion");
    check(!img.pixels.empty(), "the render still produces a real image when a progress token is supplied");
}

void testCancellationStopsRenderEarly() {
    // A big enough render that single-threaded generation takes a
    // perceptible amount of time - cancelling after the first sub-batch
    // should stop well short of pointsTarget.
    auto flame = makeSierpinskiFlame(256);
    flame->sampleDensity = 2000;

    apo::RenderProgress progress;
    // Pre-set cancelRequested before the render even starts - the
    // single-threaded loop checks it after every sub-batch, so this stops
    // after exactly one 10,000-point batch regardless of how large
    // pointsTarget is.
    progress.cancelRequested.store(true);

    apo::RenderedImage img = apo::Renderer::render(*flame, /*seed=*/11, /*threadCount=*/1, &progress);

    check(progress.pointsDone.load() < progress.pointsTarget.load(),
          "a cancelled render stops with pointsDone short of pointsTarget");
    check(progress.pointsDone.load() == 10000, "cancellation is honored after exactly one 10,000-point sub-batch");
    check(!img.pixels.empty(), "a cancelled render still returns whatever was accumulated so far, not an empty image");
}

void testPauseBlocksProgressUntilResumed() {
    // Big enough that the render is still going well after this test's own
    // sleeps below - render() blocks its caller, so it has to run on a
    // separate thread here for the test to observe it "frozen" mid-flight.
    auto flame = makeSierpinskiFlame(256);
    flame->sampleDensity = 2000;

    apo::RenderProgress progress;
    // Pre-set, same as testCancellationStopsRenderEarly's cancelRequested -
    // the single-threaded loop blocks in waitWhilePaused() right after the
    // first sub-batch, before this thread even gets a chance to race it.
    progress.pauseRequested.store(true);

    std::thread renderThread([&] { apo::Renderer::render(*flame, /*seed=*/12, /*threadCount=*/1, &progress); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const std::uint64_t doneWhilePaused = progress.pointsDone.load();
    check(doneWhilePaused == 10000, "a paused render stops right after the one sub-batch already in flight");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(progress.pointsDone.load() == doneWhilePaused, "pointsDone stays frozen for as long as the render stays paused");

    progress.pauseRequested.store(false);
    renderThread.join();

    check(progress.pointsDone.load() == progress.pointsTarget.load(),
          "clearing pauseRequested lets a paused render reach completion normally, picking up where it left off");
}

void testPauseThenCancelStopsWithoutNeedingResumeFirst() {
    auto flame = makeSierpinskiFlame(256);
    flame->sampleDensity = 2000;

    apo::RenderProgress progress;
    progress.pauseRequested.store(true);

    std::thread renderThread([&] { apo::Renderer::render(*flame, /*seed=*/13, /*threadCount=*/1, &progress); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    progress.cancelRequested.store(true); // cancel while still paused, without clearing pauseRequested first
    renderThread.join(); // must not hang waiting for a resume that never comes

    check(progress.pointsDone.load() < progress.pointsTarget.load(),
          "a render cancelled while paused stops short, without needing Resume first");
}

void testEstimatePeakMemoryScalesWithImageSizeAndThreadCount() {
    auto small = makeSierpinskiFlame(64);
    auto larger = makeSierpinskiFlame(256);

    const std::uint64_t smallEstimate = apo::Renderer::estimatePeakMemoryBytes(*small, /*threadCount=*/1);
    const std::uint64_t largerEstimate = apo::Renderer::estimatePeakMemoryBytes(*larger, /*threadCount=*/1);
    check(largerEstimate > smallEstimate, "a larger declared image size estimates more peak memory");

    const std::uint64_t moreThreadsEstimate = apo::Renderer::estimatePeakMemoryBytes(*small, /*threadCount=*/4);
    check(moreThreadsEstimate > smallEstimate,
          "more threads estimates more peak memory - each gets its own private bucket array");
    check(moreThreadsEstimate < smallEstimate * 5,
          "the threadCount scaling is linear (threadCount+1 bucket arrays), not some larger multiple");
}

void testEstimatePeakMemoryIsPositiveForATinyImage() {
    auto flame = makeSierpinskiFlame(1);
    check(apo::Renderer::estimatePeakMemoryBytes(*flame, 1) > 0,
          "even a 1x1 image estimates a nonzero amount - the filter/gutter padding around the declared canvas "
          "alone is nonzero");
}

void testEstimatePeakMemoryResolvesAutoThreadCount() {
    auto flame = makeSierpinskiFlame(64);
    const std::uint64_t autoEstimate = apo::Renderer::estimatePeakMemoryBytes(*flame, /*threadCount=*/0);
    const std::uint64_t explicitOneThread = apo::Renderer::estimatePeakMemoryBytes(*flame, /*threadCount=*/1);
    check(autoEstimate >= explicitOneThread,
          "threadCount=0 (Auto) resolves to at least 1 hardware thread, matching render()'s own convention");
}

void testTimingsAreLeftZeroWhenNotRequested() {
    // The default nullptr path (every pre-existing render() call site) must
    // stay exactly as cheap as before - nothing here to check on the
    // RenderedImage/RenderStats side beyond "it still renders", since
    // RenderTimings simply never gets touched when the caller doesn't pass
    // one.
    auto flame = makeSierpinskiFlame(64);
    flame->sampleDensity = 200;
    apo::RenderedImage img = apo::Renderer::render(*flame, /*seed=*/3, /*threadCount=*/1);
    check(!img.pixels.empty(), "render() without a RenderTimings* still renders normally");
}

void testTimingsReportPlausiblePerPhaseDurations() {
    // Large enough that iterate meaningfully dominates setup/reduce/tonemap
    // - this is what B0's apo_bench harness (src/tools/apo_bench.cpp) relies
    // on to attribute a render's wall time to a specific phase.
    auto flame = makeSierpinskiFlame(128);
    flame->sampleDensity = 300;

    apo::RenderTimings timings;
    check(timings.setupSeconds == 0 && timings.iterateSeconds == 0 && timings.reduceSeconds == 0 &&
              timings.tonemapSeconds == 0,
          "a freshly constructed RenderTimings starts all-zero");

    apo::RenderedImage img = apo::Renderer::render(*flame, /*seed=*/5, /*threadCount=*/1, /*progress=*/nullptr,
                                                     &timings);

    check(!img.pixels.empty(), "render() still produces a real image when a RenderTimings* is supplied");
    check(timings.setupSeconds >= 0 && timings.iterateSeconds >= 0 && timings.reduceSeconds >= 0 &&
              timings.tonemapSeconds >= 0,
          "every phase duration is non-negative");
    check(timings.iterateSeconds > 0, "a non-trivial single-threaded render reports nonzero iterate time");
    check(timings.iterateSeconds > timings.setupSeconds,
          "point iteration dominates plan setup for a render this size, matching what the phase split is meant to "
          "capture");
}

void testTimingsStayZeroForADegenerateFlame() {
    // render()'s early-return path (numXForms<=0 or zero width/height) never
    // reaches any of the phase boundaries - timings must stay untouched
    // (caller-owned zero-initialization), not garbage or partially filled.
    auto flame = std::make_unique<apo::Flame>();
    flame->width = 0;
    flame->height = 0;

    apo::RenderTimings timings;
    apo::RenderedImage img = apo::Renderer::render(*flame, /*seed=*/1, /*threadCount=*/1, /*progress=*/nullptr,
                                                     &timings);

    check(img.pixels.empty(), "a degenerate flame still produces an empty image with a RenderTimings* supplied");
    check(timings.setupSeconds == 0 && timings.iterateSeconds == 0 && timings.reduceSeconds == 0 &&
              timings.tonemapSeconds == 0,
          "a degenerate flame's early return leaves every RenderTimings field untouched");
}

} // namespace

int main() {
    testSingleThreadedIsDeterministic();
    testDifferentSeedsProduceDifferentOutput();
    testMultithreadedRenderCompletesAndCoversCanvas();
    testPartialOpacityStillRejectsPointsStochastically();
    testFullOpacityAcceptsEveryFinitePoint();
    testMultithreadedRenderWithMoreThreadsThanImageRows();
    testDegenerateFlameProducesEmptyImage();
    testFinalXformIsApplied();
    testDefaultCurvesDoNotChangeOutput();
    testNonDefaultMasterCurveChangesOutput();
    testCurvesSetOnlyChecksPointsNotWeights();
    testAdaptiveDEOffByDefaultEvenWithPositiveEstimator();
    testAdaptiveDERequiresPositiveEstimator();
    testAdaptiveDESmoothsNoiseInASparseLowDensityRender();
    testAdaptiveDEWithHugeEstimatorStaysBoundedAndFinishes();
    testDefaultPrecisionIsDoubleAndBitReproducible();
    testFloatBucketsProduceAValidSimilarImage();
    testFloatPrecisionHalvesTheMemoryEstimate();
    testEstimatePeakMemoryDefaultsToDouble();
    testProgressReportsTargetAndReachesCompletion();
    testCancellationStopsRenderEarly();
    testPauseBlocksProgressUntilResumed();
    testPauseThenCancelStopsWithoutNeedingResumeFirst();
    testEstimatePeakMemoryScalesWithImageSizeAndThreadCount();
    testEstimatePeakMemoryIsPositiveForATinyImage();
    testEstimatePeakMemoryResolvesAutoThreadCount();
    testTimingsAreLeftZeroWhenNotRequested();
    testTimingsReportPlausiblePerPhaseDurations();
    testTimingsStayZeroForADegenerateFlame();
    return apo_test::reportAndExit();
}
