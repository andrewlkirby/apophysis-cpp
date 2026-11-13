// Warp-uniform xform scheduling (chaosKernelWarpUniform, GpuRenderer.h's
// XformScheduling): all 32 threads of a warp apply the same xform each step
// and points are shuffled between warps, to avoid warp divergence on flames
// whose xforms run different variation lists. Checks that:
//   - Auto picks it only for divergent, xaos-free flames, and a forced
//     request falls back to the per-thread kernel when there is xaos;
//   - it renders the same image as the per-thread kernel (statistically -
//     the RNG streams differ), and is no further from the CPU renderer;
//   - every sub-batch still produces exactly kDeviceSubBatchSize points,
//     including the partial last block of a launch;
//   - float buckets and hybrid (CPU-assist) mode work with it.
//
// Skips (not fails) when no CUDA device is available or this binary wasn't
// built with APO_ENABLE_CUDA - same convention as gpu_progress_control_test.cpp.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <thread>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/VariationRegistry.h"
#include "core/io/ImageCompare.h"
#include "core/render/Renderer.h"
#include "core/render/gpu/GpuAvailability.h"

#ifdef APO_HAVE_CUDA
#include "core/render/gpu/GpuRenderer.h"
#endif

using apo_test::check;

namespace {

#ifdef APO_HAVE_CUDA
using apo::gpu::GpuRenderer;
using apo::gpu::XformScheduling;

// Same bar as hybrid_render_test.cpp: two GPU renders of one flame with
// independent RNG streams agree far above this, while points landing in
// the wrong place or with the wrong color would not.
constexpr double kMinPsnrDb = 35.0;

void setGrayscaleColorMap(apo::Flame& flame) {
    for (int i = 0; i < 256; ++i) {
        flame.cmap.entries[i][0] = static_cast<std::uint8_t>(i);
        flame.cmap.entries[i][1] = static_cast<std::uint8_t>(255 - i);
        flame.cmap.entries[i][2] = static_cast<std::uint8_t>((i * 3) & 255);
        flame.cmap.entries[i][3] = 255;
    }
}

std::unique_ptr<apo::Flame> baseFlame(int width, int height, double density) {
    auto flame = std::make_unique<apo::Flame>();
    flame->width = width;
    flame->height = height;
    flame->spatialOversample = 1;
    flame->sampleDensity = density;
    flame->center = {0.0, 0.0};
    flame->pixelsPerUnit = width / 4.0;
    flame->whiteLevel = 200;
    setGrayscaleColorMap(*flame);
    return flame;
}

int varIndex(const char* name) { return apo::VariationRegistry::instance().variationIndex(name); }

void setAffine(apo::XForm& xf, const double (&c)[6]) {
    xf.c[0] = {c[0], c[3]};
    xf.c[1] = {c[1], c[4]};
    xf.c[2] = {c[2], c[5]};
}

// Four xforms with four different variation lists - the divergent case.
// `bounded` keeps every variation finite for any finite input (no julian),
// so pointsGenerated can be checked exactly.
std::unique_ptr<apo::Flame> makeDivergentFlame(int width, int height, double density, bool bounded) {
    auto flame = baseFlame(width, height, density);
    const double coefs[4][6] = {
        {0.5, 0.0, -0.5, 0.0, 0.5, -0.4},
        {0.6, 0.4, -0.4, 0.6, 0.3, -0.15},
        {-0.5, 0.5, 0.5, 0.5, -0.1, 0.25},
        {0.4, -0.3, 0.2, 0.4, 0.3, 0.4},
    };
    for (int i = 0; i < 4; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0 + 0.25 * i;
        xf.color = i / 3.0;
        setAffine(xf, coefs[i]);
    }
    // xform 0 keeps its default linear.
    flame->xform[1]->setVariation(0, 0.0);
    flame->xform[1]->setVariation(varIndex("swirl"), 0.7);
    flame->xform[2]->setVariation(0, 0.0);
    flame->xform[2]->setVariation(varIndex("sinusoidal"), 1.0);
    flame->xform[3]->setVariation(0, 0.3);
    flame->xform[3]->setVariation(bounded ? varIndex("sinusoidal") : varIndex("julian"), 0.8);
    if (!bounded) flame->xform[3]->setVariation(varIndex("swirl"), 0.2);
    return flame;
}

// Three xforms, one shared variation list - nothing to diverge on.
std::unique_ptr<apo::Flame> makeUniformFlame(int width, int height, double density) {
    auto flame = baseFlame(width, height, density);
    const double coefs[3][6] = {
        {0.7, -0.3, 0.3, 0.7, -0.2, 0.1},
        {0.6, 0.4, -0.4, 0.6, 0.3, -0.15},
        {-0.5, 0.5, 0.5, 0.5, -0.1, 0.25},
    };
    for (int i = 0; i < 3; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.color = i / 2.0;
        setAffine(xf, coefs[i]);
        xf.setVariation(0, 0.0);
        xf.setVariation(varIndex("swirl"), 0.6);
        xf.setVariation(varIndex("julian"), 1.0);
    }
    return flame;
}

double psnr(const apo::RenderedImage& a, const apo::RenderedImage& b) {
    if (a.pixels.empty() || b.pixels.empty() || a.width != b.width || a.height != b.height ||
        a.channels != b.channels) {
        return -1.0;
    }
    return apo::computePsnr(a.width, a.height, a.channels, a.pixels.data(), b.pixels.data());
}

void testResolve() {
    auto divergent = makeDivergentFlame(640, 480, 400.0, false);
    auto uniform = makeUniformFlame(640, 480, 400.0);
    check(GpuRenderer::resolveScheduling(*divergent) == XformScheduling::WarpUniform,
          "resolve: Auto picks warp-uniform for xforms with different variation lists");
    check(GpuRenderer::resolveScheduling(*uniform) == XformScheduling::PerThread,
          "resolve: Auto keeps per-thread when every xform runs the same variation list");
    check(GpuRenderer::resolveScheduling(*uniform, XformScheduling::WarpUniform) == XformScheduling::WarpUniform,
          "resolve: warp-uniform can be forced on a non-divergent flame");
    check(GpuRenderer::resolveScheduling(*divergent, XformScheduling::PerThread) == XformScheduling::PerThread,
          "resolve: per-thread can be forced");

    // Xaos: xform 0 may never be followed by xform 1.
    divergent->xform[0]->modWeights[1] = 0.0;
    check(GpuRenderer::resolveScheduling(*divergent) == XformScheduling::PerThread,
          "resolve: Auto keeps per-thread with xaos");
    check(GpuRenderer::resolveScheduling(*divergent, XformScheduling::WarpUniform) == XformScheduling::PerThread,
          "resolve: a forced warp-uniform request falls back to per-thread with xaos");
}

void testMatchesPerThread(apo::BucketPrecision precision, const char* label) {
    auto flame = makeDivergentFlame(640, 480, 400.0, false);
    const apo::RenderedImage perThread =
        GpuRenderer::render(*flame, /*seed=*/3, nullptr, nullptr, precision, 0, XformScheduling::PerThread);
    const apo::RenderedImage warp =
        GpuRenderer::render(*flame, /*seed=*/3, nullptr, nullptr, precision, 0, XformScheduling::WarpUniform);
    const double db = psnr(warp, perThread);
    char msg[192];
    std::snprintf(msg, sizeof(msg), "%s: warp-uniform vs per-thread PSNR %.2f dB (>= %.1f)", label, db, kMinPsnrDb);
    check(db >= kMinPsnrDb, msg);

    // Same flame, same totals: both kernels lose points only to genuine
    // non-finite bailouts, so their counts should be close.
    const double gen = static_cast<double>(warp.stats.pointsGenerated);
    const double ref = static_cast<double>(perThread.stats.pointsGenerated);
    std::snprintf(msg, sizeof(msg), "%s: points generated %.0f vs per-thread %.0f (within 1%%)", label, gen, ref);
    check(ref > 0 && std::abs(gen - ref) <= 0.01 * ref, msg);
}

// The shuffle correlates points within a warp for a step at a time; if that
// visibly biased the image, the warp-uniform render would sit further from
// the CPU renderer's than the per-thread one does.
void testNoFurtherFromCpu() {
    auto flame = makeDivergentFlame(640, 480, 400.0, false);
    const apo::RenderedImage cpu = apo::Renderer::render(*flame, /*seed=*/11, 0);
    const apo::RenderedImage perThread =
        GpuRenderer::render(*flame, /*seed=*/11, nullptr, nullptr, apo::BucketPrecision::Double, 0,
                            XformScheduling::PerThread);
    const apo::RenderedImage warp =
        GpuRenderer::render(*flame, /*seed=*/11, nullptr, nullptr, apo::BucketPrecision::Double, 0,
                            XformScheduling::WarpUniform);
    const double dbThread = psnr(cpu, perThread);
    const double dbWarp = psnr(cpu, warp);
    char msg[192];
    std::snprintf(msg, sizeof(msg), "vs CPU: warp-uniform %.2f dB, per-thread %.2f dB (warp within 1 dB)", dbWarp,
                  dbThread);
    check(dbThread > 0 && dbWarp >= dbThread - 1.0, msg);
}

// Every sub-batch produces exactly kDeviceSubBatchSize points when nothing
// goes non-finite - including threads in a launch's partial last block,
// which start as dead slots in the shuffle.
//
// Run small (one partial wave, sub-batches spanning several launches as the
// step count ramps up) and large (several waves - more sub-batches than the
// GPU keeps resident - with a partial last wave), for both kernels.
void testExactPointCount() {
    struct Case {
        int width, height;
        double density;
        const char* label;
    };
    const Case cases[] = {{800, 600, 200.0, "one wave"}, {1920, 1080, 250.0, "several waves"}};
    for (const Case& c : cases) {
        auto flame = makeDivergentFlame(c.width, c.height, c.density, true);
        for (XformScheduling mode : {XformScheduling::WarpUniform, XformScheduling::PerThread}) {
            apo::RenderProgress progress;
            const apo::RenderedImage img =
                GpuRenderer::render(*flame, /*seed=*/5, &progress, nullptr, apo::BucketPrecision::Double, 0, mode);
            const std::uint64_t target = progress.pointsTarget.load();
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                          "count (%s, %s): pointsGenerated %llu == pointsDone %llu == pointsTarget %llu "
                          "(%llu sub-batches)",
                          c.label, mode == XformScheduling::WarpUniform ? "warp-uniform" : "per-thread",
                          static_cast<unsigned long long>(img.stats.pointsGenerated),
                          static_cast<unsigned long long>(progress.pointsDone.load()),
                          static_cast<unsigned long long>(target), static_cast<unsigned long long>(target / 10000));
            check(target > 0 && img.stats.pointsGenerated == target && progress.pointsDone.load() == target, msg);
        }
    }
}

void testHybrid() {
    auto flame = makeDivergentFlame(640, 480, 400.0, false);
    const int threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    const apo::RenderedImage gpuOnly =
        GpuRenderer::render(*flame, /*seed=*/7, nullptr, nullptr, apo::BucketPrecision::Double, 0,
                            XformScheduling::WarpUniform);
    const apo::RenderedImage hybrid =
        GpuRenderer::render(*flame, /*seed=*/7, nullptr, nullptr, apo::BucketPrecision::Double, threads,
                            XformScheduling::WarpUniform);
    const double db = psnr(hybrid, gpuOnly);
    char msg[192];
    std::snprintf(msg, sizeof(msg), "hybrid: warp-uniform GPU + CPU vs GPU-only PSNR %.2f dB (>= %.1f)", db,
                  kMinPsnrDb);
    check(db >= kMinPsnrDb, msg);
}
#endif

} // namespace

int main() {
    if (!apo::gpu::isCudaAvailable()) {
        check(true, ("skipped: " + apo::gpu::cudaUnavailableReason()).c_str());
        return apo_test::reportAndExit();
    }

#ifdef APO_HAVE_CUDA
    testResolve();
    testMatchesPerThread(apo::BucketPrecision::Double, "double");
    testMatchesPerThread(apo::BucketPrecision::Float, "float");
    testNoFurtherFromCpu();
    testExactPointCount();
    testHybrid();
#endif

    return apo_test::reportAndExit();
}
