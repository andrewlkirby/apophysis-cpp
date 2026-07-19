#include "Renderer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

#include "../Bezier.h"
#include "../Point3.h"
#include "../Rng.h"
#include "../XForm.h"

namespace apo {
namespace {

// ---------------------------------------------------------------------
// Constants (from Flame/ControlPoint.pas / Rendering/RenderingInterface.pas
// / Rendering/ImageMaker.pas - verified directly against source, not just
// the design brief, since these are load-bearing for correctness).
// ---------------------------------------------------------------------
constexpr int kSubBatchSize = 10000;
constexpr int kPropTableSize = 1024;

// How long a paused render's threads sleep between rechecking
// pauseRequested/cancelRequested - see RenderProgress::pauseRequested's own
// doc comment. Short enough that Resume/Cancel feel immediate, long enough
// that a long pause costs effectively no CPU (this is a sleep, not a spin).
constexpr auto kPausePollInterval = std::chrono::milliseconds(20);

// Blocks the calling thread while `progress->pauseRequested` is set, without
// touching any of that thread's render state (ThreadState/buckets stay
// exactly as they were) - so the caller's own per-batch loop can just resume
// iterating once this returns, same as if it had never stopped. Returns
// immediately (including when `progress` is null) if not paused, or the
// instant cancelRequested is set - every call site still re-checks
// cancelRequested itself right after, since "was paused, now cancelled" and
// "was never paused" both exit this function the same way.
void waitWhilePaused(RenderProgress* progress) {
    if (!progress) return;
    while (progress->pauseRequested.load(std::memory_order_relaxed) &&
           !progress->cancelRequested.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(kPausePollInterval);
    }
}
constexpr int kFuse = 15; // "for i := 0 to FUSE" -> 16 warm-up iterations
constexpr int kMaxFilterWidth = 25;
constexpr double kFilterCutoff = 1.8;
constexpr double kBrightAdjust = 2.3;
constexpr double kPrefilterWhite = static_cast<double>(1 << 26);

// Scalar-templated (FOLLOWUP_PLAN.txt B5): Scalar=double is the original,
// unchanged-behavior bucket (32 bytes) every existing caller still gets by
// default; Scalar=float (16 bytes) is a new, explicitly opt-in path (see
// Renderer::render()'s BucketPrecision parameter) - half the working set
// and write traffic of the chaos game's random-access bucket writes (the
// classic flame-fractal cache-miss bottleneck), at the cost of float's own
// well-known tradeoffs: `count` saturates (stops incrementing) once a
// single bucket accumulates more than 2^24 (~16.7M) points - a real
// concern for extreme-density final renders concentrating at attractor hot
// spots, a non-issue for the low-density interactive previews this is
// actually wired into - and accumulated color channels round differently
// than double's, which is why this must never silently become any
// existing caller's default (see every BucketPrecision parameter below
// defaulting to Double).
template <class Scalar>
struct BucketT {
    Scalar red = 0, green = 0, blue = 0, count = 0;
};
using BucketD = BucketT<double>;

enum class ProjectionKind { None, Pitch, PitchYaw, PitchDOF, PitchYawDOF };

// CameraMatrix[a,b] in Pascal (ControlPoint.pas's 3x3 CameraMatrix) - named
// fields instead of a 2D array so the projection formulas below read the
// same as the Pascal source line-for-line.
struct CameraMatrix {
    double m00 = 1, m10 = 0, m20 = 0;
    double m01 = 0, m11 = 1, m21 = 0;
    double m02 = 0, m12 = 0, m22 = 1;
};

CameraMatrix buildCameraMatrix(double pitch, double yaw) {
    CameraMatrix m;
    m.m00 = std::cos(-yaw);
    m.m10 = -std::sin(-yaw);
    m.m20 = 0;
    m.m01 = std::cos(pitch) * std::sin(-yaw);
    m.m11 = std::cos(pitch) * std::cos(-yaw);
    m.m21 = -std::sin(pitch);
    m.m02 = std::sin(pitch) * std::sin(-yaw);
    m.m12 = std::sin(pitch) * std::cos(-yaw);
    m.m22 = std::cos(pitch);
    return m;
}

ProjectionKind selectProjectionKind(const Flame& flame) {
    if (flame.cameraDOF != 0) {
        return (flame.cameraYaw != 0) ? ProjectionKind::PitchYawDOF : ProjectionKind::PitchDOF;
    }
    if (flame.cameraPitch != 0 || flame.cameraYaw != 0) {
        return (flame.cameraYaw != 0) ? ProjectionKind::PitchYaw : ProjectionKind::Pitch;
    }
    return ProjectionKind::None;
}

// 3D -> 2D camera projection, matching ControlPoint.pas's
// ProjectNone/ProjectPitch/ProjectPitchYaw/ProjectPitchDOF/ProjectPitchYawDOF
// exactly (verified directly against source, including the DOF variants'
// reuse of the rotated-z term as the DOF blur radius scale - the original's
// GAUSSIAN_DOF branch is dead code, never defined in the shipped build, so
// only the uniform-random-disk blur shown here is live). `rng` is only
// consumed by the two DOF variants (2 draws each), and only for points that
// already passed the opacity accept roll - matching the original's call
// ordering, so rejected points never pay for it.
//
// Templated on the RNG type, not `Rng&` (FOLLOWUP_PLAN.txt B2a) - its sole
// caller (renderBatch) now holds a concrete Xoshiro256Rng, and passing that
// through unchanged (rather than upcasting to the abstract Rng&) lets the
// compiler devirtualize/inline the DOF branches' two uniform01() draws the
// same way renderBatch's own direct draws already are. Only ever
// instantiated for Xoshiro256Rng in this single-translation-unit,
// internal-linkage function - no code-bloat concern.
template <class RngT>
void project(Point3& pt, ProjectionKind kind, double persp, double zpos, double dofCoef, const CameraMatrix& m,
             RngT& rng) {
    const double origZ = pt.z;
    switch (kind) {
        case ProjectionKind::None: {
            const double zr = 1 - persp * (pt.z - zpos);
            pt.x /= zr;
            pt.y /= zr;
            pt.z = origZ - zpos;
            break;
        }
        case ProjectionKind::Pitch: {
            const double zPrime = pt.z - zpos;
            const double yRot = m.m11 * pt.y + m.m21 * zPrime;
            const double zRot = m.m12 * pt.y + m.m22 * zPrime;
            const double zr = 1 - persp * zRot;
            pt.x = pt.x / zr;
            pt.y = yRot / zr;
            pt.z = origZ - zpos;
            break;
        }
        case ProjectionKind::PitchYaw: {
            const double zPrime = pt.z - zpos;
            const double xRot = m.m00 * pt.x + m.m10 * pt.y;
            const double yRot = m.m01 * pt.x + m.m11 * pt.y + m.m21 * zPrime;
            const double zRot = m.m02 * pt.x + m.m12 * pt.y + m.m22 * zPrime;
            const double zr = 1 - persp * zRot;
            pt.x = xRot / zr;
            pt.y = yRot / zr;
            pt.z = origZ - zpos;
            break;
        }
        case ProjectionKind::PitchDOF: {
            const double zPrime = pt.z - zpos;
            const double yRot = m.m11 * pt.y + m.m21 * zPrime;
            const double zRot = m.m12 * pt.y + m.m22 * zPrime;
            const double zr = 1 - persp * zRot;
            const double t = rng.uniform01() * kTwoPi;
            const double dsin = std::sin(t), dcos = std::cos(t);
            const double dr = rng.uniform01() * dofCoef * zRot;
            pt.x = (pt.x + dr * dcos) / zr;
            pt.y = (yRot + dr * dsin) / zr;
            pt.z = origZ - zpos;
            break;
        }
        case ProjectionKind::PitchYawDOF: {
            const double zPrime = pt.z - zpos;
            const double xRot = m.m00 * pt.x + m.m10 * pt.y;
            const double yRot = m.m01 * pt.x + m.m11 * pt.y + m.m21 * zPrime;
            const double zRot = m.m02 * pt.x + m.m12 * pt.y + m.m22 * zPrime;
            const double zr = 1 - persp * zRot;
            const double t = rng.uniform01() * kTwoPi;
            const double dsin = std::sin(t), dcos = std::cos(t);
            const double dr = rng.uniform01() * dofCoef * zRot;
            pt.x = (xRot + dr * dcos) / zr;
            pt.y = (yRot + dr * dsin) / zr;
            pt.z = origZ - zpos;
            break;
        }
    }
}

// Proportional xform-selection table: propTable[k*kPropTableSize + slot]
// is the index of the xform to jump to next, given the chain is currently
// at xform k, weighted by density*modWeights (xaos). Matches
// ControlPoint.pas's Prepare() PropTable build exactly (verified directly
// against source).
std::vector<int> buildPropTable(const Flame& flame, int numXForms) {
    std::vector<int> table(static_cast<size_t>(numXForms) * kPropTableSize, 0);
    std::vector<double> tp(numXForms);

    for (int k = 0; k < numXForms; ++k) {
        double total = 0;
        for (int i = 0; i < numXForms; ++i) {
            tp[i] = flame.xform[i]->density * flame.xform[k]->modWeights[i];
            total += tp[i];
        }

        int* row = &table[static_cast<size_t>(k) * kPropTableSize];
        if (total > 0) {
            double loopValue = 0;
            for (int i = 0; i < kPropTableSize; ++i) {
                double propsum = 0;
                int j = -1;
                do {
                    ++j;
                    propsum += tp[j];
                } while (!(propsum > loopValue || j == numXForms - 1));
                row[i] = j;
                loopValue += total / kPropTableSize;
            }
        }
        // else: leave this row zero-filled (fallback to xform 0). The
        // original points every slot at a dedicated always-inert
        // invalidXform placeholder for this case (every xform reachable
        // from k has a zero xaos weight) - xform 0 is always a real, valid
        // xform (Flame::numXForms()'s packing contract guarantees at least
        // one), so this is a simpler equivalent for what's already a
        // deliberately-degenerate-weights edge case.
    }
    return table;
}

// Prepared once per render, shared (read-only) across every worker thread.
struct RenderPlan {
    int numXForms = 0;
    bool useFinalXform = false;
    std::vector<int> propTable;

    ProjectionKind projectionKind = ProjectionKind::None;
    CameraMatrix cameraMatrix;
    double persp = 0, zpos = 0, dofCoef = 0;

    double camX0 = 0, camY0 = 0, camW = 1, camH = 1;
    double bws = 1, bhs = 1;
    int bucketWidth = 0, bucketHeight = 0;
    int oversample = 1;
    int gutterWidth = 0, maxGutterWidth = 0;

    std::array<std::array<double, 3>, 256> colorMap{};

    int numBatches = 1;

    // Tone-mapping (ImageMaker.pas's CreateFilter/CreateImage) - computed
    // here too since filter size feeds gutter/buffer sizing above.
    int filterSize = 1;
    std::vector<double> filter; // filterSize*filterSize, row-major
    bool fastBucket = true;
    double k1 = 0, k2 = 0;
    std::array<double, 1025> logScale{};

    // Bezier tone curves (see buildCurveTables) - index 0 is the master/
    // "All" channel, 1/2/3 are Red/Green/Blue, each a 257-entry (0..256
    // inclusive) lookup table. curvesSet mirrors ImageMaker.pas's own
    // check: false (skip curve application entirely) only while every
    // channel is still at its exact default identity configuration.
    bool curvesSet = false;
    std::array<std::array<double, 257>, 4> curveTable{};

    // Adaptive density estimation (see buildAdaptiveDE/adaptiveRadiusFor
    // and Renderer.h's class comment on why this is a from-scratch design,
    // not a port). deKernels[r] (r=1..deMaxRadiusInt) is a normalized
    // (2r+1)x(2r+1) circular Gaussian kernel, row-major; index 0 is unused
    // (the minimum meaningful radius is 1).
    bool adaptiveDE = false;
    double deMinRadius = 1.0, deMaxRadius = 1.0, deCurve = 0.2;
    int deMaxRadiusInt = 1;
    std::vector<std::vector<double>> deKernels;
};

// Matches ImageMaker.pas's CreateFilter exactly (verified directly against
// source): a normalized Gaussian-ish kernel, sized from spatialOversample
// and spatialFilterRadius.
void buildFilter(const Flame& flame, RenderPlan& plan) {
    const int oversample = plan.oversample;
    const int fw = static_cast<int>(2.0 * kFilterCutoff * oversample * flame.spatialFilterRadius);
    int filterSize = fw + 1;
    if ((filterSize + oversample) % 2 != 0) ++filterSize; // match oversample's parity

    const double adjust = (fw > 0) ? (kFilterCutoff * filterSize) / fw : 1.0;

    plan.filterSize = filterSize;
    plan.filter.assign(static_cast<size_t>(filterSize) * filterSize, 0.0);
    double sum = 0;
    for (int i = 0; i < filterSize; ++i) {
        for (int j = 0; j < filterSize; ++j) {
            const double ii = ((2.0 * i + 1.0) / filterSize - 1.0) * adjust;
            const double jj = ((2.0 * j + 1.0) / filterSize - 1.0) * adjust;
            const double v = std::exp(-2.0 * (ii * ii + jj * jj));
            plan.filter[static_cast<size_t>(i) * filterSize + j] = v;
            sum += v;
        }
    }
    if (sum != 0) {
        for (double& v : plan.filter) v /= sum;
    }
}

// Matches RenderingInterface.pas's CalcBufferSize + CreateCamera exactly
// (verified directly against source).
void buildBuffersAndCamera(const Flame& flame, RenderPlan& plan) {
    plan.oversample = flame.spatialOversample;
    plan.maxGutterWidth = (kMaxFilterWidth - plan.oversample) / 2;
    plan.gutterWidth = (plan.filterSize - plan.oversample) / 2;
    plan.bucketWidth = plan.oversample * flame.width + 2 * plan.maxGutterWidth;
    plan.bucketHeight = plan.oversample * flame.height + 2 * plan.maxGutterWidth;
    // ImageMaker.CreateImage recomputes "gutter_width" locally as
    // (FBucketWidth - FOversample*Width), which algebraically equals
    // 2*maxGutterWidth - i.e. the FastBucket check is really "does this
    // render's actual filter fit inside the worst-case-sized gutter the
    // buffer was allocated with", not the per-render gutterWidth above.
    plan.fastBucket = plan.filterSize <= plan.maxGutterWidth;

    const double scale = std::pow(2.0, flame.zoom);
    const double sampleDensity = flame.sampleDensity * scale * scale;
    const double ppux = flame.pixelsPerUnit * scale;
    const double ppuy = flame.pixelsPerUnit * scale;

    const double cornerX = flame.center[0] - flame.width / ppux / 2.0;
    const double cornerY = flame.center[1] - flame.height / ppuy / 2.0;
    const double t0 = plan.gutterWidth / (plan.oversample * ppux);
    const double t1 = plan.gutterWidth / (plan.oversample * ppuy);
    const double t2 = (2 * plan.maxGutterWidth - plan.gutterWidth) / (plan.oversample * ppux);
    const double t3 = (2 * plan.maxGutterWidth - plan.gutterWidth) / (plan.oversample * ppuy);

    plan.camX0 = cornerX - t0;
    plan.camY0 = cornerY - t1;
    const double camX1 = cornerX + flame.width / ppux + t2;
    const double camY1 = cornerY + flame.height / ppuy + t3;
    plan.camW = camX1 - plan.camX0;
    plan.camH = camY1 - plan.camY0;

    const double xSize = (std::fabs(plan.camW) > 0.01) ? (1.0 / plan.camW) : 1.0;
    const double ySize = (std::fabs(plan.camH) > 0.01) ? (1.0 / plan.camH) : 1.0;
    plan.bws = (plan.bucketWidth - 0.5) * xSize;
    plan.bhs = (plan.bucketHeight - 0.5) * ySize;

    // NSamples/NumBatches (RenderingInterface.pas's InitBuffers, verified
    // directly against source).
    const long long bucketCount = static_cast<long long>(plan.bucketWidth) * plan.bucketHeight;
    const double nSamples = std::round(sampleDensity * static_cast<double>(bucketCount) /
                                        (plan.oversample * plan.oversample));
    const int nbatches = std::max(1, flame.nbatches);
    plan.numBatches = std::max(1, static_cast<int>(std::round(nSamples / (nbatches * kSubBatchSize))));
}

// Matches RenderingInterface.pas's CreateColorMap exactly: the 256-entry
// palette scaled by white_level, precomputed once per render rather than
// per point.
void buildColorMap(const Flame& flame, RenderPlan& plan) {
    for (int i = 0; i < 256; ++i) {
        plan.colorMap[i][0] = static_cast<double>(flame.cmap.entries[i][0] * flame.whiteLevel) / 256.0;
        plan.colorMap[i][1] = static_cast<double>(flame.cmap.entries[i][1] * flame.whiteLevel) / 256.0;
        plan.colorMap[i][2] = static_cast<double>(flame.cmap.entries[i][2] * flame.whiteLevel) / 256.0;
    }
}

// Matches ImageMaker.pas's CreateImage tone-map setup exactly (k1/k2 and
// the 1025-entry log-scale lookup table, verified directly against
// source). `actualDensity` mirrors fcp.actual_density - for a single,
// complete (non-incremental) render this is just flame.sampleDensity.
void buildToneMap(const Flame& flame, RenderPlan& plan) {
    const double scale2 = std::pow(2.0, flame.zoom);
    double sampleDensity = flame.sampleDensity * scale2 * scale2;
    if (sampleDensity == 0) sampleDensity = 0.001;

    plan.k1 = (flame.contrast * kBrightAdjust * flame.brightness * 268 * kPrefilterWhite) / 256.0;
    const double ppux = flame.pixelsPerUnit * scale2;
    const double ppuy = flame.pixelsPerUnit * scale2;
    const double area = static_cast<double>(flame.width) * flame.height / (ppux * ppuy);
    plan.k2 = (plan.oversample * plan.oversample) / (flame.contrast * area * flame.whiteLevel * sampleDensity);

    plan.logScale[0] = 0;
    for (int i = 1; i <= 1024; ++i) {
        plan.logScale[i] =
            (plan.k1 * std::log10(1 + flame.whiteLevel * i * plan.k2)) / (flame.whiteLevel * i);
    }
}

// Matches ImageMaker.pas's csa[] lookup-table construction and its
// curvesSet detection (both verified directly against source) - index 0
// is the master/"All" channel, applied first; 1/2/3 are Red/Green/Blue,
// applied after. Skipping curve application entirely when every channel
// is still at its exact default identity configuration isn't just a perf
// shortcut: under this scheme (see Bezier.h's evalBezierCurve doc
// comment), the default configuration doesn't evaluate to a numerically-
// identity table, so skipping it is required for correctness, not only
// speed - confirmed directly (see bezier_test.cpp's
// testDefaultCurveIsTheDiagonalLine, which shows the default curve *is*
// exactly y=x as a parametric curve, even though sampling y(t) at
// t=i/256 does not reproduce that as a per-i identity table).
//
// Only checks curvePoints, not curveWeights, matching the original
// exactly - a curve edited via weight only (dragging the weight slider
// without ever moving a control point away from the default (0,0)/(1,1)
// position) is invisible at render time, in this port as in the
// original; not fixed, since it's an obscure interaction quirk rather
// than a data-loss or crash-class bug.
void buildCurveTables(const Flame& flame, RenderPlan& plan) {
    static const BezierCurve kIdentity{};
    plan.curvesSet = false;
    for (const auto& curve : flame.curves) {
        if (curve.points != kIdentity.points) {
            plan.curvesSet = true;
            break;
        }
    }
    if (!plan.curvesSet) return;

    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i <= 256; ++i) {
            plan.curveTable[c][i] = evalBezierCurve(flame.curves[c], i / 256.0) * 256.0;
        }
    }
}

// Precomputes buildAdaptiveDE's per-integer-radius kernels and the radius
// bounds a per-pixel density maps into - see Renderer.h's class comment
// for why this is a from-scratch design (the original's own adaptive-DE
// code is unreachable dead code) rather than a port. Kept simple and
// bounded on purpose: radii are capped at plan.maxGutterWidth (the gutter
// the fixed filter's own buffer sizing already reserves - see
// buildBuffersAndCamera), so this never needs to grow the render buffer or
// prove a wider gutter is safe, and kernels are precomputed once per
// integer radius (not per pixel) so the per-pixel tone-map loop only ever
// does a table lookup, not a fresh Gaussian evaluation.
void buildAdaptiveDE(const Flame& flame, RenderPlan& plan) {
    plan.adaptiveDE = flame.enableDE && flame.estimator > 0;
    if (!plan.adaptiveDE) return;

    const double cap = static_cast<double>(std::max(1, plan.maxGutterWidth));
    const double maxRadiusRaw = flame.estimator * plan.oversample + 1.0;
    const double minRadiusRaw = std::max(0.0, flame.estimatorMin) * plan.oversample + 1.0;
    plan.deMaxRadius = std::clamp(maxRadiusRaw, 1.0, cap);
    plan.deMinRadius = std::clamp(std::min(minRadiusRaw, plan.deMaxRadius), 1.0, cap);
    // A zero/negative curve would make the density->radius falloff (see
    // adaptiveRadiusFor) degenerate (either flat or inverted) - fall back
    // to a gentle default rather than letting a bad .flame value break
    // monotonicity.
    plan.deCurve = (flame.estimatorCurve > 0) ? flame.estimatorCurve : 0.2;

    plan.deMaxRadiusInt = std::max(1, static_cast<int>(std::ceil(plan.deMaxRadius)));
    plan.deKernels.assign(static_cast<size_t>(plan.deMaxRadiusInt) + 1, {});
    for (int r = 1; r <= plan.deMaxRadiusInt; ++r) {
        const int size = 2 * r + 1;
        std::vector<double> kernel(static_cast<size_t>(size) * size, 0.0);
        double sum = 0;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                // Circular support (clipped at d=1), not the fixed filter's
                // square window - a natural fit for "this pixel's estimated
                // influence radius is r", and this path isn't constrained
                // to match ImageMaker.pas's square-windowed formula (there's
                // no live original behavior to match here - see above).
                const double di = (i - r) / static_cast<double>(r);
                const double dj = (j - r) / static_cast<double>(r);
                const double d2 = di * di + dj * dj;
                const double v = (d2 <= 1.0) ? std::exp(-2.0 * d2) : 0.0;
                kernel[static_cast<size_t>(i) * size + j] = v;
                sum += v;
            }
        }
        if (sum > 0) {
            for (double& v : kernel) v /= sum;
        }
        plan.deKernels[static_cast<size_t>(r)] = std::move(kernel);
    }
}

// Maps a pixel's local (raw, un-tone-mapped) bucket density to a blur
// radius: count=0 -> deMaxRadius (widest blur, for sparse/noisy regions),
// growing density shrinks the radius toward deMinRadius, shaped by
// deCurve. Strictly monotonically non-increasing in `count` for any
// deCurve > 0, and bounded to [deMinRadius, deMaxRadius] for every
// count >= 0.
double adaptiveRadiusFor(const RenderPlan& plan, double count) {
    const double span = plan.deMaxRadius - plan.deMinRadius;
    if (span <= 0) return plan.deMaxRadius;
    const double t = 1.0 / std::pow(1.0 + std::max(0.0, count), plan.deCurve);
    return plan.deMinRadius + span * t;
}

// Runs fn(begin, end) across up to `threadCount` roughly-equal contiguous
// slices of [0, totalWork) and waits for all of them - FOLLOWUP_PLAN.txt's
// B4 (parallelize the two leftover single-threaded passes: bucket reduction
// and tone-mapping). Deliberately a static contiguous split, not the
// atomic-work-stealing counter renderBatch's own multithreaded path uses:
// that exists there because one SUB_BATCH_SIZE batch's cost varies with how
// many points it ends up rejecting, so idle threads stealing the next batch
// keeps threads balanced; reduction (one bucket = fixed numSources adds) and
// tone-mapping (one row = a fixed-size filter convolution) are uniform-cost
// per unit of work, so a plain static split needs no synchronization at all
// and costs nothing extra to set up. threadCount<=1 or totalWork<=1 calls
// fn(0, totalWork) directly with no thread spawned.
template <typename F>
void parallelFor(int threadCount, long long totalWork, F&& fn) {
    threadCount = std::max(1, threadCount);
    if (threadCount == 1 || totalWork <= 1) {
        fn(0LL, totalWork);
        return;
    }
    threadCount = static_cast<int>(std::min<long long>(threadCount, totalWork));
    const long long chunk = (totalWork + threadCount - 1) / threadCount;
    std::vector<std::jthread> workers;
    workers.reserve(static_cast<size_t>(threadCount));
    for (int t = 0; t < threadCount; ++t) {
        const long long begin = static_cast<long long>(t) * chunk;
        const long long end = std::min(totalWork, begin + chunk);
        if (begin >= end) break;
        workers.emplace_back([&fn, begin, end] { fn(begin, end); });
    }
    // jthreads join automatically as `workers` goes out of scope here.
}

// Per-thread mutable state: an independent Flame clone (each XForm's
// per-point scratch fields and bound Rng* would otherwise race across
// threads - see Renderer.h/Flame::clone()'s comments), its own RNG stream,
// and a private full-size bucket array accumulated into lock-free.
//
// rng is a concrete Xoshiro256Rng by value, not unique_ptr<Rng>
// (FOLLOWUP_PLAN.txt B2a) - renderBatch's own direct draws (the propTable
// selection, the initial point, the opacity roll) and project()'s DOF
// branches are the hottest, most frequent Rng::uniform01() call sites in
// the whole render, and a value member with a statically-known concrete
// type lets the compiler devirtualize/inline those calls instead of
// dispatching through Rng's vtable on every one. XForm::prepare() still
// takes the abstract Rng& (XForm's stochastic local variations and
// registered-variation Variation::rng stay on that boundary deliberately -
// see the plan's own scoping for why: it's a much broader, riskier surface
// to specialize, and not what this item measured).
template <class Scalar>
struct ThreadState {
    std::unique_ptr<Flame> flame;
    Xoshiro256Rng rng;
    std::vector<BucketT<Scalar>> buckets;
};

template <class Scalar>
void accumulate(std::vector<BucketT<Scalar>>& buckets, const RenderPlan& plan, double x, double y,
                 double colorCoord) {
    const double px = x - plan.camX0;
    if (px < 0 || px > plan.camW) return;
    const double py = y - plan.camY0;
    if (py < 0 || py > plan.camH) return;

    int bx = static_cast<int>(std::lround(plan.bws * px));
    int by = static_cast<int>(std::lround(plan.bhs * py));
    bx = std::clamp(bx, 0, plan.bucketWidth - 1);
    by = std::clamp(by, 0, plan.bucketHeight - 1);

    int colorIdx = static_cast<int>(std::lround(colorCoord * 255));
    colorIdx = std::clamp(colorIdx, 0, 255);

    const auto& mapColor = plan.colorMap[colorIdx];
    BucketT<Scalar>& b = buckets[static_cast<size_t>(by) * plan.bucketWidth + bx];
    b.red += static_cast<Scalar>(mapColor[0]);
    b.green += static_cast<Scalar>(mapColor[1]);
    b.blue += static_cast<Scalar>(mapColor[2]);
    b.count += 1;
}

// One SUB_BATCH_SIZE chaos-game batch: FUSE warm-up, then per-point
// selection/iteration/opacity-roll/projection/accumulation, matching
// ControlPoint.pas's IterateXYC (verified directly against source) - minus
// the intermediate Points[] buffer and shared critical section (see
// Renderer.h's comment on why: buckets are lock-free-private-per-thread
// here, so there's no reason to defer accumulation to a second pass).
template <class Scalar>
void renderBatch(ThreadState<Scalar>& ts, const RenderPlan& plan, RenderStats& stats) {
    // Concrete Xoshiro256Rng&, not Rng& (FOLLOWUP_PLAN.txt B2a) - see
    // ThreadState's own comment on why.
    Xoshiro256Rng& rng = ts.rng;
    auto& xforms = ts.flame->xform;

    Point3 p{2 * rng.uniform01() - 1, 2 * rng.uniform01() - 1, 0};
    double c = rng.uniform01();
    int xfIdx = 0;

    for (int i = 0; i <= kFuse; ++i) {
        xfIdx = plan.propTable[static_cast<size_t>(xfIdx) * kPropTableSize + rng.uniformInt(kPropTableSize)];
        xforms[xfIdx]->nextPoint(p, c);
    }

    for (int i = 0; i < kSubBatchSize; ++i) {
        xfIdx = plan.propTable[static_cast<size_t>(xfIdx) * kPropTableSize + rng.uniformInt(kPropTableSize)];
        XForm& xf = *xforms[xfIdx];
        xf.nextPoint(p, c);

        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || !std::isfinite(c)) {
            // Matches the original's EMathError bailout (a domain error
            // inside a variation, e.g. ln of a negative number) aborting
            // the rest of this batch rather than continuing to chain from
            // a poisoned point - but only this one batch, not the thread's
            // remaining work, and with no stale leftover buffer to worry
            // about (see the two-phase-vs-single-phase comment above).
            break;
        }
        ++stats.pointsGenerated;

        // FOLLOWUP_PLAN.txt B3: transOpacity==1 (the overwhelming common
        // case for real flames) means this roll can never reject - any
        // uniform01() draw in [0,1) is < 1.0 - so skip drawing from the RNG
        // at all rather than drawing a value whose outcome is already
        // certain. CAVEAT this item's own plan text flags: this removes a
        // draw from the RNG stream for every point through a
        // transOpacity==1 xform, changing the single-threaded deterministic
        // output versus before - baselines were regenerated in this same
        // change (see tests/baselines/), not left stale.
        if (!xf.opacityAlwaysPasses() && rng.uniform01() >= xf.transOpacity) continue; // rejected by opacity roll

        Point3 q = p;
        double qc = c;
        if (plan.useFinalXform) {
            ts.flame->finalXform->nextPoint(q, qc);
        }
        project(q, plan.projectionKind, plan.persp, plan.zpos, plan.dofCoef, plan.cameraMatrix, rng);

        if (!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) || !std::isfinite(qc)) continue;

        accumulate(ts.buckets, plan, q.x, q.y, qc);
        ++stats.pointsAccepted;
    }
}

// Sums every thread's private bucket array into one (see ThreadState's own
// comment on why each worker thread owns a full private array: lock-free
// accumulation during iteration). Safe to parallelize across the bucket
// index range (see parallelFor's own comment) because each output bucket's
// value depends only on that same index across every source array - no
// cross-index dependency, and summing `numSources` doubles in the same
// t=0..numSources-1 order as the old single-threaded loop regardless of
// which thread does it keeps output bit-identical (only *which bucket
// indices* get reduced by which thread changes, never the order values are
// added together within one bucket).
template <class Scalar>
std::vector<BucketT<Scalar>> reduceBuckets(std::vector<ThreadState<Scalar>>& threadStates, int numSources,
                                            long long bucketCount, int reduceThreads) {
    if (numSources == 1) {
        // A "reduction" of exactly one source is that source itself
        // (accumulating from a zero-initialized Bucket{} against one array
        // is exactly a copy) - moved out rather than copied, and skips
        // reduceBuckets' general path entirely (single-threaded renders are
        // the deterministic/reproducible path - see Renderer.h - so this
        // also keeps that path's cost identical to before B4).
        return std::move(threadStates[0].buckets);
    }

    std::vector<BucketT<Scalar>> buckets(static_cast<size_t>(bucketCount), BucketT<Scalar>{});
    parallelFor(reduceThreads, bucketCount, [&](long long begin, long long end) {
        for (long long i = begin; i < end; ++i) {
            BucketT<Scalar> sum{};
            for (int t = 0; t < numSources; ++t) {
                const BucketT<Scalar>& src = threadStates[static_cast<size_t>(t)].buckets[static_cast<size_t>(i)];
                sum.red += src.red;
                sum.green += src.green;
                sum.blue += src.blue;
                sum.count += src.count;
            }
            buckets[static_cast<size_t>(i)] = sum;
        }
    });
    return buckets;
}

// Matches ImageMaker.pas's SafeGetBucket: clamps into bounds instead of
// indexing directly - only used when `plan.fastBucket` is false (the
// gutter isn't provably wide enough for the current filter to never read
// out of bounds).
template <class Scalar>
const BucketT<Scalar>& safeGetBucket(const std::vector<BucketT<Scalar>>& buckets, const RenderPlan& plan, int x,
                                      int y) {
    x = std::clamp(x, 0, plan.bucketWidth - 1);
    y = std::clamp(y, 0, plan.bucketHeight - 1);
    return buckets[static_cast<size_t>(y) * plan.bucketWidth + x];
}

double logScaleFor(const RenderPlan& plan, const Flame& flame, double count) {
    if (count < 1024) return plan.logScale[static_cast<size_t>(std::lround(count))];
    return (plan.k1 * std::log10(1 + flame.whiteLevel * count * plan.k2)) / (flame.whiteLevel * count);
}

// Matches ImageMaker.pas's CreateImage fixed-filter path exactly (verified
// directly against source) - gamma/vibrancy/background compositing, and
// Bezier tone curves (see buildCurveTables), per pixel. Adaptive density
// estimation (see buildAdaptiveDE/adaptiveRadiusFor - a from-scratch
// design, not a port; see Renderer.h's class comment for why) layers on
// top of the fixed-filter fp[] computed here as a refinement, not a
// separate code path.
//
// threadCount (B4): rows are independent - each row only reads `buckets`
// (read-only, shared) and writes its own disjoint slice of image.pixels, so
// splitting the row range across threadCount threads (see parallelFor)
// needs no synchronization and produces byte-identical output to the
// single-threaded loop this replaced, just reordered across rows (which
// don't interact at all).
template <class Scalar>
RenderedImage toneMap(const Flame& flame, const RenderPlan& plan, const std::vector<BucketT<Scalar>>& buckets,
                       int threadCount) {
    RenderedImage image;
    image.width = flame.width;
    image.height = flame.height;
    image.channels = flame.transparency ? 4 : 3;
    if (image.width <= 0 || image.height <= 0) return image;
    image.pixels.assign(static_cast<size_t>(image.width) * image.height * image.channels, 0);

    const double gammaExp = (flame.gamma == 0) ? 0 : (1.0 / flame.gamma);
    const double funcVal = (flame.gammaThreshold != 0) ? std::pow(flame.gammaThreshold, gammaExp - 1) : 0;
    const int vib = static_cast<int>(std::lround(flame.vibrancy * 256));
    const int notVib = 256 - vib;

    parallelFor(threadCount, image.height, [&](long long rowBegin, long long rowEnd) {
    for (int row = static_cast<int>(rowBegin); row < static_cast<int>(rowEnd); ++row) {
        const int by = row * plan.oversample + plan.maxGutterWidth;
        for (int col = 0; col < image.width; ++col) {
            const int bx = col * plan.oversample + plan.maxGutterWidth;

            double fp[4] = {0, 0, 0, 0};
            if (plan.filterSize > 1) {
                for (int ii = 0; ii < plan.filterSize; ++ii) {
                    for (int jj = 0; jj < plan.filterSize; ++jj) {
                        const double filterValue = plan.filter[static_cast<size_t>(ii) * plan.filterSize + jj];
                        const BucketT<Scalar>& bucket =
                            plan.fastBucket ? buckets[static_cast<size_t>(by + ii) * plan.bucketWidth + bx + jj]
                                             : safeGetBucket(buckets, plan, bx + jj, by + ii);
                        const double ls = logScaleFor(plan, flame, bucket.count);
                        fp[0] += filterValue * ls * bucket.red;
                        fp[1] += filterValue * ls * bucket.green;
                        fp[2] += filterValue * ls * bucket.blue;
                        fp[3] += filterValue * ls * bucket.count;
                    }
                }
                fp[0] /= kPrefilterWhite;
                fp[1] /= kPrefilterWhite;
                fp[2] /= kPrefilterWhite;
                fp[3] = flame.whiteLevel * fp[3] / kPrefilterWhite;
            } else {
                const BucketT<Scalar>& bucket = plan.fastBucket
                                                     ? buckets[static_cast<size_t>(by) * plan.bucketWidth + bx]
                                                     : safeGetBucket(buckets, plan, bx, by);
                const double ls = logScaleFor(plan, flame, bucket.count) / kPrefilterWhite;
                fp[0] = ls * bucket.red;
                fp[1] = ls * bucket.green;
                fp[2] = ls * bucket.blue;
                fp[3] = ls * bucket.count * flame.whiteLevel;
            }

            // Adaptive density estimation (see buildAdaptiveDE/
            // adaptiveRadiusFor and Renderer.h's class comment) - a
            // refinement layered on top of the fixed-filter fp[] just
            // computed above, not a replacement of that computation:
            // gated on fp[3] > 0 so pixels the fixed filter already found
            // to be pure background skip the extra work entirely, exactly
            // the same short-circuit shape ImageMaker.pas's (dead) code
            // used for the same reason.
            if (plan.adaptiveDE && fp[3] > 0) {
                const BucketT<Scalar>& centerBucket =
                    plan.fastBucket ? buckets[static_cast<size_t>(by) * plan.bucketWidth + bx]
                                     : safeGetBucket(buckets, plan, bx, by);
                const double radius = adaptiveRadiusFor(plan, centerBucket.count);
                const int r = std::clamp(static_cast<int>(std::lround(radius)), 1, plan.deMaxRadiusInt);
                const auto& kernel = plan.deKernels[static_cast<size_t>(r)];
                const int size = 2 * r + 1;

                double dp[4] = {0, 0, 0, 0};
                for (int ii = 0; ii < size; ++ii) {
                    for (int jj = 0; jj < size; ++jj) {
                        const double kv = kernel[static_cast<size_t>(ii) * size + jj];
                        if (kv == 0) continue;
                        const BucketT<Scalar>& bucket = safeGetBucket(buckets, plan, bx + jj - r, by + ii - r);
                        const double ls = logScaleFor(plan, flame, bucket.count);
                        dp[0] += kv * ls * bucket.red;
                        dp[1] += kv * ls * bucket.green;
                        dp[2] += kv * ls * bucket.blue;
                        dp[3] += kv * ls * bucket.count;
                    }
                }
                fp[0] = dp[0] / kPrefilterWhite;
                fp[1] = dp[1] / kPrefilterWhite;
                fp[2] = dp[2] / kPrefilterWhite;
                fp[3] = flame.whiteLevel * dp[3] / kPrefilterWhite;
            }

            int r = 0, g = 0, b = 0, a = 0;
            if (fp[3] > 0) {
                double alpha;
                if (fp[3] <= flame.gammaThreshold) {
                    const double frac = fp[3] / flame.gammaThreshold;
                    alpha = (1 - frac) * fp[3] * funcVal + frac * std::pow(fp[3], gammaExp);
                } else {
                    alpha = std::pow(fp[3], gammaExp);
                }

                const double ls = vib * alpha / fp[3];
                a = std::clamp(static_cast<int>(std::lround(alpha * 256)), 0, 255);
                r = static_cast<int>(std::lround(ls * fp[0] + notVib * std::pow(fp[0], gammaExp)));
                g = static_cast<int>(std::lround(ls * fp[1] + notVib * std::pow(fp[1], gammaExp)));
                b = static_cast<int>(std::lround(ls * fp[2] + notVib * std::pow(fp[2], gammaExp)));

                // Matches ImageMaker.pas's curve application exactly:
                // master/"All" curve first, then the per-channel curve on
                // top, both before background compositing/un-premultiply.
                // Unlike the original (which indexes csa[1..3] with
                // Round(csa[0][ri]) and no bounds check at all - a genuine
                // out-of-bounds array read for a hand-edited .flame file
                // with a curve shaped to overshoot [0,256]), the master-
                // curve index is clamped here before the second lookup -
                // fixed, not reproduced, the same category of issue as
                // GradientOps.h's frequencyGradient fix.
                if (plan.curvesSet) {
                    auto applyCurve = [&plan](int channel, int raw) {
                        if (raw < 0 || raw > 256) return raw;
                        const int masterIdx =
                            std::clamp(static_cast<int>(std::lround(plan.curveTable[0][raw])), 0, 256);
                        return static_cast<int>(std::lround(plan.curveTable[channel][masterIdx]));
                    };
                    r = applyCurve(1, r);
                    g = applyCurve(2, g);
                    b = applyCurve(3, b);
                }

                if (!flame.transparency) {
                    r += ((255 - a) * flame.background[0]) >> 8;
                    g += ((255 - a) * flame.background[1]) >> 8;
                    b += ((255 - a) * flame.background[2]) >> 8;
                } else if (a > 0) {
                    r = r * 255 / a;
                    g = g * 255 / a;
                    b = b * 255 / a;
                }
                r = std::clamp(r, 0, 255);
                g = std::clamp(g, 0, 255);
                b = std::clamp(b, 0, 255);
            } else if (!flame.transparency) {
                r = flame.background[0];
                g = flame.background[1];
                b = flame.background[2];
                a = 255;
            }

            const size_t pixelOffset = (static_cast<size_t>(row) * image.width + col) * image.channels;
            image.pixels[pixelOffset + 0] = static_cast<std::uint8_t>(r);
            image.pixels[pixelOffset + 1] = static_cast<std::uint8_t>(g);
            image.pixels[pixelOffset + 2] = static_cast<std::uint8_t>(b);
            if (image.channels == 4) image.pixels[pixelOffset + 3] = static_cast<std::uint8_t>(a);
        }
    }
    });

    return image;
}

// The full render pipeline, templated on the bucket scalar type
// (FOLLOWUP_PLAN.txt B5) - Renderer::render() below is a thin, non-template
// dispatcher that picks Scalar=double (every existing caller's unchanged
// behavior) or Scalar=float (new, explicitly opt-in) based on the caller's
// BucketPrecision argument. Otherwise identical to render()'s own body
// before this item.
template <class Scalar>
RenderedImage renderImpl(const Flame& flame, std::uint64_t seed, int threadCount, RenderProgress* progress,
                          RenderTimings* timings) {
    using clock = std::chrono::steady_clock;
    const auto secondsBetween = [](clock::time_point a, clock::time_point b) {
        return std::chrono::duration<double>(b - a).count();
    };
    const auto tStart = clock::now();

    const int numXForms = flame.numXForms();
    if (numXForms <= 0 || flame.width <= 0 || flame.height <= 0) {
        return RenderedImage{};
    }

    RenderPlan plan;
    plan.numXForms = numXForms;
    plan.useFinalXform = flame.finalXformEnabled && flame.useFinalXform;
    plan.propTable = buildPropTable(flame, numXForms);

    plan.projectionKind = selectProjectionKind(flame);
    plan.cameraMatrix = buildCameraMatrix(flame.cameraPitch, flame.cameraYaw);
    plan.persp = flame.cameraPersp;
    plan.zpos = flame.cameraZpos;
    plan.dofCoef = 0.1 * flame.cameraDOF;

    buildFilter(flame, plan);
    buildBuffersAndCamera(flame, plan);
    buildColorMap(flame, plan);
    buildToneMap(flame, plan);
    buildCurveTables(flame, plan);
    buildAdaptiveDE(flame, plan);

    const int requestedThreads =
        threadCount > 0 ? threadCount : std::max(1u, std::thread::hardware_concurrency());

    std::vector<ThreadState<Scalar>> threadStates(static_cast<size_t>(requestedThreads));
    for (int t = 0; t < requestedThreads; ++t) {
        ThreadState<Scalar>& ts = threadStates[t];
        // cloneForRender(), not clone() (FOLLOWUP_PLAN.txt B1a) - a render's
        // per-thread clone only ever touches xform[0..numXForms-1] and
        // finalXform (see buildPropTable()'s own index range and every
        // ts.flame-> access below), so there's no reason to pay clone()'s
        // full 101-slot instantiate-then-assign cost per thread when a real
        // flame typically uses a handful of xforms.
        ts.flame = flame.cloneForRender(numXForms);
        // Distinct per-thread seeds derived from the caller's seed via
        // splitmix-style mixing (not just seed+t: adjacent integer seeds
        // fed straight into a raw LCG-like stream can correlate early
        // output - Xoshiro256Rng's own constructor already reseeds via
        // splitmix64 internally, so seed+t here is fine as an *input* to
        // that, it just needs to be distinct per thread, which it is).
        ts.rng = Xoshiro256Rng(seed + static_cast<std::uint64_t>(t) * 0x9e3779b97f4a7c15ULL);
        ts.buckets.assign(static_cast<size_t>(plan.bucketWidth) * plan.bucketHeight, BucketT<Scalar>{});

        if (flame.soloXform >= 0 && flame.soloXform < numXForms) {
            for (int i = 0; i < numXForms; ++i) ts.flame->xform[i]->transOpacity = 0;
            ts.flame->xform[flame.soloXform]->transOpacity = 1;
        }

        // XForm::prepare() still takes the abstract Rng& (B2a's own
        // scoping - see ThreadState's comment): ts.rng upcasts implicitly
        // here, same as passing *unique_ptr<Rng> did before.
        for (int i = 0; i < numXForms; ++i) ts.flame->xform[i]->prepare(ts.rng);
        if (plan.useFinalXform) ts.flame->finalXform->prepare(ts.rng);
    }

    std::vector<RenderStats> threadStats(static_cast<size_t>(requestedThreads));

    if (progress) {
        progress->pointsDone.store(0, std::memory_order_relaxed);
        progress->pointsTarget.store(static_cast<std::uint64_t>(plan.numBatches) * kSubBatchSize,
                                      std::memory_order_relaxed);
    }

    const auto tSetupEnd = clock::now();

    if (requestedThreads == 1) {
        // No thread spawned at all for the common "give me a deterministic
        // render" case - not just "one worker thread", to avoid any
        // thread-creation overhead when the caller specifically wants the
        // single-threaded, bit-reproducible path (see Renderer.h).
        for (int b = 0; b < plan.numBatches; ++b) {
            renderBatch(threadStates[0], plan, threadStats[0]);
            if (progress) {
                progress->pointsDone.fetch_add(kSubBatchSize, std::memory_order_relaxed);
                if (progress->cancelRequested.load(std::memory_order_relaxed)) break;
                waitWhilePaused(progress);
                if (progress->cancelRequested.load(std::memory_order_relaxed)) break;
            }
        }
    } else {
        std::atomic<int> nextBatch{0};
        // A C++ thread function that exits via an uncaught exception calls
        // std::terminate() immediately, on that OS thread, regardless of any
        // try/catch further up the call chain on the thread that spawned it
        // (that's a different stack entirely) - so a std::bad_alloc from
        // renderBatch (large canvas colliding with whatever else is using
        // memory right now) inside one of these workers would otherwise take
        // the whole process down uncatchably. Capturing it into
        // firstError/firstErrorFlag and rethrowing after every worker has
        // joined moves that exception onto *this* thread - RenderWorker's own
        // try/catch (see RenderWorker.h) can then actually catch it.
        std::exception_ptr firstError;
        std::atomic<bool> firstErrorFlag{false};
        std::mutex errorMutex;
        std::vector<std::jthread> workers;
        workers.reserve(requestedThreads);
        for (int t = 0; t < requestedThreads; ++t) {
            workers.emplace_back([&, t] {
                int batch;
                while ((batch = nextBatch.fetch_add(1, std::memory_order_relaxed)) < plan.numBatches) {
                    if (progress && progress->cancelRequested.load(std::memory_order_relaxed)) break;
                    if (firstErrorFlag.load(std::memory_order_relaxed)) break;
                    try {
                        renderBatch(threadStates[t], plan, threadStats[t]);
                    } catch (...) {
                        if (!firstErrorFlag.exchange(true, std::memory_order_relaxed)) {
                            std::lock_guard<std::mutex> lock(errorMutex);
                            firstError = std::current_exception();
                        }
                        if (progress) progress->cancelRequested.store(true, std::memory_order_relaxed);
                        break;
                    }
                    if (progress) {
                        progress->pointsDone.fetch_add(kSubBatchSize, std::memory_order_relaxed);
                        waitWhilePaused(progress);
                        if (progress->cancelRequested.load(std::memory_order_relaxed)) break;
                    }
                }
            });
        }
        // jthreads join automatically as `workers` goes out of scope below;
        // explicit here anyway for clarity about the synchronization point
        // before the bucket-reduction pass reads every thread's buckets.
        for (auto& w : workers) w.join();
        if (firstError) std::rethrow_exception(firstError);
    }

    const auto tIterateEnd = clock::now();

    // Reduction: sum every thread's private bucket array into one (see
    // reduceBuckets' own comment for why this is safe to parallelize - B4).
    const long long bucketCount = static_cast<long long>(plan.bucketWidth) * plan.bucketHeight;
    std::vector<BucketT<Scalar>> buckets =
        reduceBuckets(threadStates, requestedThreads, bucketCount, requestedThreads);
    RenderStats totalStats;
    for (int t = 0; t < requestedThreads; ++t) {
        totalStats.pointsGenerated += threadStats[t].pointsGenerated;
        totalStats.pointsAccepted += threadStats[t].pointsAccepted;
    }

    const auto tReduceEnd = clock::now();

    RenderedImage image = toneMap(flame, plan, buckets, requestedThreads);
    image.stats = totalStats;

    if (timings) {
        const auto tToneMapEnd = clock::now();
        timings->setupSeconds = secondsBetween(tStart, tSetupEnd);
        timings->iterateSeconds = secondsBetween(tSetupEnd, tIterateEnd);
        timings->reduceSeconds = secondsBetween(tIterateEnd, tReduceEnd);
        timings->tonemapSeconds = secondsBetween(tReduceEnd, tToneMapEnd);
    }
    return image;
}

} // namespace

RenderedImage Renderer::render(const Flame& flame, std::uint64_t seed, int threadCount, RenderProgress* progress,
                                RenderTimings* timings, BucketPrecision precision) {
    if (precision == BucketPrecision::Float) {
        return renderImpl<float>(flame, seed, threadCount, progress, timings);
    }
    return renderImpl<double>(flame, seed, threadCount, progress, timings);
}

std::uint64_t Renderer::estimatePeakMemoryBytes(const Flame& flame, int threadCount, BucketPrecision precision) {
    // Reuses buildFilter/buildBuffersAndCamera exactly as render() itself
    // calls them (same order - buildBuffersAndCamera reads plan.filterSize,
    // set by buildFilter), so this can never silently drift out of sync
    // with the real allocation math as those formulas evolve - deliberately
    // NOT a second, hand-copied formula living in the UI layer.
    RenderPlan plan;
    buildFilter(flame, plan);
    buildBuffersAndCamera(flame, plan);

    const int resolvedThreads = threadCount > 0 ? threadCount : std::max(1u, std::thread::hardware_concurrency());
    // (resolvedThreads) private per-thread bucket arrays (see this class's
    // own header comment on the lock-free-private-bucket design) plus 1
    // more for the final reduction buffer every thread's buckets get
    // summed into (see this function's caller, render(), a few lines
    // above `std::vector<BucketT<Scalar>> buckets(...)`). Every other
    // allocation (the filter kernel, per-thread cloned Flame/XForm objects,
    // output pixels, tone-map lookup tables) is comparatively negligible -
    // a few KB to a few MB even at extreme settings - so omitted from this
    // estimate; the bucket arrays dominate by orders of magnitude for any
    // non-trivial image size.
    const std::uint64_t bucketArea =
        static_cast<std::uint64_t>(plan.bucketWidth) * static_cast<std::uint64_t>(plan.bucketHeight);
    // sizeof(BucketD) (double) unless the caller opted into Float - see
    // BucketPrecision's own doc comment (Renderer.h) for the tradeoff.
    const std::uint64_t bucketBytes =
        precision == BucketPrecision::Float ? sizeof(BucketT<float>) : sizeof(BucketD);
    return bucketArea * bucketBytes * static_cast<std::uint64_t>(resolvedThreads + 1);
}

std::vector<std::pair<double, double>> Renderer::samplePoints(const Flame& flame, std::uint64_t seed, int count) {
    const int numXForms = flame.numXForms();
    if (numXForms <= 0 || count <= 0) return {};

    // cloneForRender(), not clone() (FOLLOWUP_PLAN.txt B1a) - same
    // [0,numXForms)+finalXform-only access pattern as render()'s own
    // per-thread clone above.
    auto sampleFlame = flame.cloneForRender(numXForms);
    Xoshiro256Rng rng(seed);
    for (int i = 0; i < numXForms; ++i) sampleFlame->xform[i]->prepare(rng);
    const bool useFinal = sampleFlame->finalXformEnabled && sampleFlame->useFinalXform;
    if (useFinal) sampleFlame->finalXform->prepare(rng);

    const std::vector<int> propTable = buildPropTable(*sampleFlame, numXForms);

    std::vector<std::pair<double, double>> points;
    points.reserve(static_cast<size_t>(count));

    Point3 p{2 * rng.uniform01() - 1, 2 * rng.uniform01() - 1, 0};
    double c = rng.uniform01();
    int xfIdx = 0;
    for (int i = 0; i <= kFuse; ++i) {
        xfIdx = propTable[static_cast<size_t>(xfIdx) * kPropTableSize + rng.uniformInt(kPropTableSize)];
        sampleFlame->xform[xfIdx]->nextPoint(p, c);
    }

    for (int i = 0; i < count; ++i) {
        xfIdx = propTable[static_cast<size_t>(xfIdx) * kPropTableSize + rng.uniformInt(kPropTableSize)];
        XForm& xf = *sampleFlame->xform[xfIdx];
        xf.nextPoint(p, c);
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || !std::isfinite(c)) break;
        if (xf.transOpacity == 0.0) continue; // matches IterateXY's own hard opacity=0 exclusion

        double qx = p.x, qy = p.y;
        if (useFinal) {
            Point3 q = p;
            double qc = c;
            sampleFlame->finalXform->nextPoint(q, qc);
            qx = q.x;
            qy = q.y;
        }
        if (std::isfinite(qx) && std::isfinite(qy)) points.emplace_back(qx, qy);
    }
    return points;
}

} // namespace apo
