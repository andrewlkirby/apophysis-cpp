// Phase 5's correctness harness: renders each fixture in tests/baselines/
// deterministically (single-threaded, fixed seed) and compares against its
// checked-in reference PNG via PSNR. This is what the migration plan's
// Phase 5 actually asks for - "wire this as an automated check that runs
// after every variation/render change... this is the thing that will
// actually catch a mistranslated variation formula" - just without a real
// Delphi build to generate *golden* references from (see
// OPTIMIZATION_STATUS.md / the Phase 4 summary: that's still blocked on
// having an actual Delphi install somewhere). These baselines instead
// protect against *regressions* relative to this port's own prior output -
// still valuable on its own (an unintended change to any variation, the
// camera projection, xaos selection, or tone-mapping will show up as a
// PSNR drop here), and the harness is ready to point at real Apophysis
// output the moment that's available, without changing how the comparison
// itself works.
//
// The five fixtures were chosen to each exercise a different part of the
// renderer that a regression could plausibly hide in:
//   - simple_sierpinski: pure affine + bucket accumulation, no variations
//   - swirl_spherical: variation blending (multiple weighted variations
//     per xform)
//   - xaos_blend: proportional xform selection with per-xform xaos
//     (`chaos=`) weights, not just density
//   - camera_3d: the pitch/yaw/perspective/DOF projection path (Renderer.cpp's
//     ProjectionKind::PitchYawDOF)
//   - final_xform: the optional finalXform application
//
// A very high (not infinite) PSNR threshold is used rather than exact
// byte equality: single-threaded rendering is deterministic *within one
// build*, but a different libpng version or compiler could legitimately
// shift a handful of channel values by 1 during PNG's own lossless
// encode/decode round trip without indicating any real regression - see
// ImageCompare.h's own comment on this.

#include <cstdio>
#include <string>
#include <vector>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/io/FlameIO.h"
#include "core/io/ImageCompare.h"
#include "core/io/PngWriter.h"
#include "core/render/Renderer.h"

using apo_test::check;

namespace {

// Baseline files live in the source tree (checked in, meant to be
// inspected/updated deliberately), not the build tree - CMake stamps the
// absolute source path in via this macro (see tests/CMakeLists.txt) so the
// test works regardless of ctest's working directory.
#ifndef APO_BASELINES_DIR
#define APO_BASELINES_DIR "."
#endif

constexpr double kMinPsnrDb = 50.0; // very high bar - see file header comment

void checkFixture(const std::string& name) {
    const std::string flamePath = std::string(APO_BASELINES_DIR) + "/" + name + ".flame";
    const std::string baselinePngPath = std::string(APO_BASELINES_DIR) + "/" + name + ".png";

    auto flames = apo::loadFlameFile(flamePath);
    if (!check(flames.size() == 1, ("baseline fixture '" + name + "' loads as one flame").c_str())) return;

    apo::RenderedImage fresh = apo::Renderer::render(*flames[0], /*seed=*/1, /*threadCount=*/1);
    if (!check(!fresh.pixels.empty(), ("baseline fixture '" + name + "' renders a non-empty image").c_str())) return;

    int baseWidth = 0, baseHeight = 0;
    std::vector<std::uint8_t> basePixelsRgba;
    if (!check(apo::readPng(baselinePngPath, baseWidth, baseHeight, basePixelsRgba),
               ("baseline PNG for '" + name + "' reads back successfully").c_str())) {
        return;
    }

    if (!check(fresh.width == baseWidth && fresh.height == baseHeight,
               ("fresh render matches baseline dimensions for '" + name + "'").c_str())) {
        return;
    }

    // Both sides compared as RGBA: readPng always decodes to 4 channels
    // (see PngWriter.h), so a fresh 3-channel (opaque) render needs its
    // own alpha channel synthesized (all 255) before the two buffers are
    // comparable byte-for-byte.
    std::vector<std::uint8_t> freshRgba;
    if (fresh.channels == 4) {
        freshRgba = fresh.pixels;
    } else {
        freshRgba.resize(static_cast<size_t>(fresh.width) * fresh.height * 4);
        for (size_t p = 0; p < static_cast<size_t>(fresh.width) * fresh.height; ++p) {
            freshRgba[p * 4 + 0] = fresh.pixels[p * 3 + 0];
            freshRgba[p * 4 + 1] = fresh.pixels[p * 3 + 1];
            freshRgba[p * 4 + 2] = fresh.pixels[p * 3 + 2];
            freshRgba[p * 4 + 3] = 255;
        }
    }

    const double psnr = apo::computePsnr(fresh.width, fresh.height, 4, freshRgba.data(), basePixelsRgba.data());
    char msg[256];
    std::snprintf(msg, sizeof(msg), "'%s' matches its baseline (PSNR=%.1f dB, threshold=%.0f dB)", name.c_str(),
                  psnr, kMinPsnrDb);
    check(psnr >= kMinPsnrDb, msg);
}

} // namespace

int main() {
    for (const char* name :
         {"simple_sierpinski", "swirl_spherical", "xaos_blend", "camera_3d", "final_xform"}) {
        checkFixture(name);
    }
    return apo_test::reportAndExit();
}
