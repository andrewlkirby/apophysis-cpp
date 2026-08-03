// Whole-flame CPU-vs-GPU parity for the CUDA render backend
// (src/core/render/gpu/ - see docs/GPU_RENDERING_PLAN.md), exercised against
// the same checked-in fixtures baseline_regression_test.cpp uses
// (tests/baselines/*.flame), rather than gpu_variation_parity_test.cpp's
// synthetic one-variation-at-a-time flames. Where that file proves each
// variation's math individually, this file proves the surrounding machinery
// that only shows up when a real flame is rendered whole: xaos/chaos-
// weighted xform selection (xaos_blend), multi-variation blending within one
// xform (swirl_spherical), the 3D pitch/yaw/perspective/DOF camera
// projection (camera_3d), and the optional final xform (final_xform) - plus
// a plain affine IFS (simple_sierpinski) as the floor case. Every one of
// these fixtures' variations (linear, bubble, spherical, swirl, horseshoe)
// is on VariationKinds.cpp's ported list, so all five are expected to be
// GPU-eligible; a fixture that stops being eligible (e.g. a baseline
// swapped in a not-yet-ported variation) fails loudly here rather than
// silently falling back to CPU.
//
// Not a comparison against the checked-in baseline PNGs (those are a
// single-threaded, fixed-seed *CPU* golden - see baseline_regression_test.cpp's
// own header comment on why that comparison is only meaningful CPU vs CPU).
// Like gpu_variation_parity_test.cpp, this compares two independent RNG
// streams (CPU vs GPU) via computePsnr() at the same PSNR bar, so what's
// actually being verified is "these two backends render the same attractor
// shape/coloring from this real flame file", not bit-exactness. The
// baseline files' own quality="20"-ish sample density (tuned for a fast,
// single-seed exact-reproduction test) is too low for a robust independent-
// noise comparison, so each fixture is cloned with a higher sampleDensity
// before rendering - same rationale as gpu_variation_parity_test.cpp's own
// buildTestFlame() using density=200 rather than whatever a hypothetical
// caller's flame happened to have.
//
// Skips (not fails) entirely when no CUDA device is available at runtime,
// or when this binary wasn't built with APO_ENABLE_CUDA - same convention
// as gpu_variation_parity_test.cpp.

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/io/FlameIO.h"
#include "core/io/ImageCompare.h"
#include "core/render/Renderer.h"
#include "core/render/gpu/DeviceFlame.h"
#include "core/render/gpu/GpuAvailability.h"

#ifdef APO_HAVE_CUDA
#include "core/render/gpu/GpuRenderer.h"
#endif

using apo_test::check;

namespace {

#ifndef APO_BASELINES_DIR
#define APO_BASELINES_DIR "."
#endif

constexpr double kMinPsnrDb = 15.0; // see file header - independent RNG streams, same bar as gpu_variation_parity_test
constexpr double kParityDensity = 300.0; // overrides each fixture's own low quality="..." for a robust comparison

#ifdef APO_HAVE_CUDA
void testBaselineFlameParity(const std::string& name) {
    const std::string flamePath = std::string(APO_BASELINES_DIR) + "/" + name + ".flame";
    auto flames = apo::loadFlameFile(flamePath);
    if (!check(flames.size() == 1, ("baseline fixture '" + name + "' loads as one flame").c_str())) return;

    apo::Flame& flame = *flames[0];
    flame.sampleDensity = kParityDensity;

    if (!check(apo::gpu::isFlameGpuEligible(flame), (name + ": baseline flame is GPU-eligible").c_str())) {
        return; // an unported variation snuck into a baseline fixture - nothing more to compare
    }

    const std::uint64_t seed = 321;
    apo::RenderedImage cpuImg = apo::Renderer::render(flame, seed, /*threadCount=*/1);
    apo::RenderedImage gpuImg = apo::gpu::GpuRenderer::render(flame, seed);

    if (!check(!cpuImg.pixels.empty(), (name + ": CPU render produced pixel data").c_str())) return;
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
    char msg[224];
    std::snprintf(msg, sizeof(msg), "%s: whole-flame CPU vs GPU PSNR %.1f dB >= %.1f dB threshold", name.c_str(),
                  psnr, kMinPsnrDb);
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
    for (const char* name :
         {"simple_sierpinski", "swirl_spherical", "xaos_blend", "camera_3d", "final_xform"}) {
        testBaselineFlameParity(name);
    }
#endif

    return apo_test::reportAndExit();
}
