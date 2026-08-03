// CPU-vs-GPU parity harness for the CUDA render backend
// (src/core/render/gpu/ - see docs/GPU_RENDERING_PLAN.md). Renders a small
// test flame exercising one variation at a time on both apo::Renderer (CPU,
// authoritative) and apo::gpu::GpuRenderer (GPU), and compares the two
// images with the existing computePsnr() (src/core/io/ImageCompare.h).
//
// This is NOT a bit-exactness check: the CPU and GPU backends draw from
// completely independent RNG streams (see GpuRenderer.h's own doc comment,
// and Renderer.h's pre-existing note that even CPU multithreaded rendering
// isn't bit-reproducible), so per-point chaos-game noise never matches
// exactly. What must match is the underlying attractor shape and color
// mapping - a high-density render's PSNR against the CPU reference is the
// acceptance gate for "this variation was ported correctly", same
// methodology baseline_regression_test.cpp already uses for CPU-vs-CPU
// comparisons, just at a lower bar (kMinPsnrDb below) appropriate for two
// genuinely independent noise realizations rather than a reordered-sum
// variant of the same one.
//
// Skips (not fails) entirely when no CUDA device is available at runtime,
// or when this binary wasn't built with APO_ENABLE_CUDA - both cases report
// one informational "ok" check so CI machines without an NVIDIA GPU (or a
// CUDA-disabled build, the default) still pass this test file.

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/VariationRegistry.h"
#include "core/io/ImageCompare.h"
#include "core/render/Renderer.h"
#include "core/render/gpu/DeviceFlame.h"
#include "core/render/gpu/GpuAvailability.h"

#ifdef APO_HAVE_CUDA
#include "core/render/gpu/GpuRenderer.h"
#endif

using apo_test::check;

namespace {

constexpr double kMinPsnrDb = 15.0; // see file header - independent RNG streams, not a reorder of the same one

void setGrayscaleColorMap(apo::Flame& flame) {
    for (int i = 0; i < 256; ++i) {
        flame.cmap.entries[i][0] = i;
        flame.cmap.entries[i][1] = i;
        flame.cmap.entries[i][2] = i;
        flame.cmap.entries[i][3] = 255;
    }
}

// A 3-xform IFS (same corner layout as renderer_test.cpp's
// makeSierpinskiFlame) where xform 0 uses `variationName` at full weight
// (replacing its default linear weight) and xforms 1/2 stay plain
// contracting linear maps. Keeping 2 of the 3 xforms simple/contracting
// means the chaos game stays well-bounded even for a variation whose math
// alone wouldn't obviously contract (most points still get pulled back
// toward the attractor by whichever of xforms 1/2 fires next), while xform
// 0 still gets exercised on roughly a third of all iterations - enough to
// dominate the rendered image's shape.
std::unique_ptr<apo::Flame> buildTestFlame(const std::string& variationName, int size = 96) {
    auto flame = std::make_unique<apo::Flame>();
    flame->width = size;
    flame->height = size;
    flame->spatialOversample = 1;
    flame->sampleDensity = 200;
    flame->center = {0.0, 0.0};
    flame->pixelsPerUnit = size / 3.0;
    flame->whiteLevel = 200;
    setGrayscaleColorMap(*flame);

    const double corners[3][2] = {{-0.5, -0.5}, {0.5, -0.5}, {0.0, 0.5}};
    auto& registry = apo::VariationRegistry::instance();
    const int varIndex = registry.variationIndex(variationName);

    for (int i = 0; i < 3; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.color = i / 2.0;
        xf.c[0] = {0.5, 0.0};
        xf.c[1] = {0.0, 0.5};
        xf.c[2] = {corners[i][0], corners[i][1]};
        if (i == 0 && varIndex >= 0) {
            xf.setVariation(0, 0.0); // replace, not add to, the default linear weight
            xf.setVariation(varIndex, 1.0);
        }
    }
    return flame;
}

#ifdef APO_HAVE_CUDA
void testVariationParity(const std::string& name) {
    auto flame = buildTestFlame(name);

    if (!check(apo::gpu::isFlameGpuEligible(*flame), (name + ": test flame is GPU-eligible").c_str())) {
        return; // a variation missing from VariationKinds.cpp's table - nothing more to compare
    }

    const std::uint64_t seed = 123;
    apo::RenderedImage cpuImg = apo::Renderer::render(*flame, seed, /*threadCount=*/1);
    apo::RenderedImage gpuImg = apo::gpu::GpuRenderer::render(*flame, seed);

    if (!check(!gpuImg.pixels.empty(), (name + ": GPU render produced pixel data").c_str())) return;
    if (!check(gpuImg.width == cpuImg.width && gpuImg.height == cpuImg.height &&
                   gpuImg.channels == cpuImg.channels,
               (name + ": GPU image dimensions/channels match the CPU image").c_str())) {
        return;
    }
    if (!check(gpuImg.stats.pointsAccepted > 0, (name + ": GPU render accepted at least some points").c_str())) {
        return;
    }

    const double psnr =
        apo::computePsnr(cpuImg.width, cpuImg.height, cpuImg.channels, cpuImg.pixels.data(), gpuImg.pixels.data());
    char msg[192];
    std::snprintf(msg, sizeof(msg), "%s: CPU vs GPU PSNR %.1f dB >= %.1f dB threshold", name.c_str(), psnr,
                  kMinPsnrDb);
    check(psnr >= kMinPsnrDb, msg);
}

// Float bucket precision (GpuRenderer.h's BucketPrecision::Float, mirroring
// the CPU renderer's own Float fast path - see DeviceTypes.h's DeviceBucketF
// comment) - checked separately from the main per-variation loop above
// (which exercises the Double default everywhere) since this is a
// backend-plumbing concern, not a per-variation one: one flame is enough to
// prove the float bucket array/atomics/tone-map read path all work.
void testFloatBucketPrecision() {
    auto flame = buildTestFlame("swirl");
    if (!check(apo::gpu::isFlameGpuEligible(*flame), "float-precision: test flame is GPU-eligible")) return;

    const std::uint64_t seed = 123;
    apo::RenderedImage cpuImg = apo::Renderer::render(*flame, seed, /*threadCount=*/1, /*progress=*/nullptr,
                                                       /*timings=*/nullptr, apo::BucketPrecision::Float);
    apo::RenderedImage gpuImg = apo::gpu::GpuRenderer::render(*flame, seed, /*progress=*/nullptr, /*timings=*/nullptr,
                                                               apo::BucketPrecision::Float);

    if (!check(!gpuImg.pixels.empty(), "float-precision: GPU render produced pixel data")) return;
    if (!check(gpuImg.width == cpuImg.width && gpuImg.height == cpuImg.height && gpuImg.channels == cpuImg.channels,
               "float-precision: GPU image dimensions/channels match the CPU image")) {
        return;
    }

    const double psnr =
        apo::computePsnr(cpuImg.width, cpuImg.height, cpuImg.channels, cpuImg.pixels.data(), gpuImg.pixels.data());
    char msg[192];
    std::snprintf(msg, sizeof(msg), "float-precision: CPU vs GPU PSNR %.1f dB >= %.1f dB threshold", psnr,
                  kMinPsnrDb);
    check(psnr >= kMinPsnrDb, msg);
}
#endif

} // namespace

int main() {
    if (!apo::gpu::isCudaAvailable()) {
        check(true, ("skipped: " + apo::gpu::cudaUnavailableReason()).c_str());
        return apo_test::reportAndExit();
    }

#ifdef APO_HAVE_CUDA
    // Every local variation (0..28, VariationRegistry::localVarNames()) plus
    // every registered variation ported so far (VariationKinds.cpp) - kept
    // as an explicit list, not a loop over VariationRegistry's full set, so
    // a variation that's genuinely not GPU-eligible yet (unported native
    // variation, or one of the 47 legacy C plugins) doesn't turn into a
    // spurious failure here; VariationKinds.cpp is the single source of
    // truth for what's actually ported.
    const std::vector<std::string> kLocalVariations = {
        "linear",     "flatten",   "sinusoidal", "spherical",  "swirl",         "horseshoe",
        "polar",      "disc",      "spiral",     "hyperbolic", "diamond",       "eyefish",
        "bubble",     "cylinder",  "noise",      "blur",       "gaussian_blur", "zblur",
        "blur3D",     "pre_blur",  "pre_zscale", "pre_ztranslate", "pre_rotate_x", "pre_rotate_y",
        "zscale",     "ztranslate", "zcone",     "post_rotate_x", "post_rotate_y",
    };
    const std::vector<std::string> kRegisteredVariations = {
        "auger",   "bent",   "blob",  "cosine", "cross",          "exponential", "foci",
        "heart",   "hemisphere", "lazysusan", "mobius", "pdj",    "popcorn",     "power",
        "pre_sinusoidal", "pre_spherical", "secant2", "separation", "splits", "tangent", "waves2",
        // Batch 2: "prepare()-only" variations (VariationKinds.cpp) - device
        // calc() recomputes each one's prepare()-derived constants inline
        // from the raw parameters rather than needing a host-side precompute
        // step - see DeviceVariations.cuh's own comment on this batch.
        "bipolar", "bwraps", "curl3D", "elliptic", "escher", "fan", "fan2", "log", "loonie", "ngon",
        "perspective", "polar2", "post_bwraps", "post_curl", "post_curl3D", "pre_bwraps", "pre_disc",
        "rings", "rings2", "scry", "waves", "wedge",
        // Batch 3: RNG-using and/or selectCalcFunction()-specialized
        // variations - see VariationKinds.cpp and DeviceVariations.cuh's
        // own comments on why each specialization is either skipped
        // (algebraically-equivalent fast path) or replicated (genuine
        // behavioral branch: Rectangles' divide-by-zero guard, Falloff2's
        // 3-way blurtype dispatch).
        "arch", "blade", "blur_circle", "blur_pixelize", "blur_zoom", "crop", "post_crop", "pre_crop",
        "curl", "rectangles", "epispiral", "pie", "rays", "twintrian", "julian", "juliascope", "julia3Dz",
        "julia3D", "radial_blur", "falloff2", "post_falloff2", "pre_falloff2",
    };

    for (const auto& name : kLocalVariations) testVariationParity(name);
    for (const auto& name : kRegisteredVariations) testVariationParity(name);

    testFloatBucketPrecision();
#endif

    return apo_test::reportAndExit();
}
