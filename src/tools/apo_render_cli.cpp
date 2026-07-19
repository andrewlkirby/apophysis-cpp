// apo_render_cli - headless flame renderer. This is the verification-
// backbone CLI the migration plan's Phase 5 calls for ("a headless CLI
// tool apo_render_cli file.flame out.png is the verification backbone for
// all math/render work before any UI exists") - built now, in Phase 4,
// rather than waiting, since Phases 2-4 (variations, rendering engine,
// file I/O) all becoming available together is exactly what makes this
// tool possible for the first time.
//
// Usage: apo_render_cli <input.flame> <output.png> [options]
//   --seed=N        RNG seed (default: 1)
//   --threads=N     0 = hardware_concurrency (default), 1 = deterministic
//                    single-threaded
//   --width=W       Override the flame's saved width
//   --height=H      Override the flame's saved height
//   --index=N       Which <flame> to render if the file has more than one
//                    (default: 0, the first)

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/Flame.h"
#include "core/io/FlameIO.h"
#include "core/io/PngWriter.h"
#include "core/render/Renderer.h"

namespace {

bool parseOption(const std::string& arg, const char* name, long& outValue) {
    const std::string prefix = std::string("--") + name + "=";
    if (arg.rfind(prefix, 0) != 0) return false;
    outValue = std::strtol(arg.c_str() + prefix.size(), nullptr, 10);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr,
                      "usage: %s <input.flame> <output.png> [--seed=N] [--threads=N] [--width=W] "
                      "[--height=H] [--index=N]\n",
                      argc > 0 ? argv[0] : "apo_render_cli");
        return 2;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    long seed = 1, threads = 0, width = -1, height = -1, index = 0;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (parseOption(arg, "seed", seed)) continue;
        if (parseOption(arg, "threads", threads)) continue;
        if (parseOption(arg, "width", width)) continue;
        if (parseOption(arg, "height", height)) continue;
        if (parseOption(arg, "index", index)) continue;
        std::fprintf(stderr, "unrecognized option: %s\n", arg.c_str());
        return 2;
    }

    auto flames = apo::loadFlameFile(inputPath);
    if (flames.empty()) {
        std::fprintf(stderr, "error: could not load any <flame> from '%s'\n", inputPath.c_str());
        return 1;
    }
    if (index < 0 || static_cast<size_t>(index) >= flames.size()) {
        std::fprintf(stderr, "error: --index=%ld out of range (file has %zu flame(s))\n", index, flames.size());
        return 1;
    }

    apo::Flame& flame = *flames[static_cast<size_t>(index)];
    if (width > 0) flame.width = static_cast<int>(width);
    if (height > 0) flame.height = static_cast<int>(height);

    const std::string threadsDesc =
        threads == 1 ? "1 (deterministic)" : (threads == 0 ? "auto" : std::to_string(threads));
    std::fprintf(stderr, "rendering '%s' (%dx%d, %d xforms%s) seed=%ld threads=%s...\n",
                 flame.name.empty() ? inputPath.c_str() : flame.name.c_str(), flame.width, flame.height,
                 flame.numXForms(), flame.finalXformEnabled ? "+final" : "", seed, threadsDesc.c_str());

    apo::RenderedImage image =
        apo::Renderer::render(flame, static_cast<std::uint64_t>(seed), static_cast<int>(threads));

    if (image.pixels.empty()) {
        std::fprintf(stderr, "error: render produced an empty image (no active xforms, or zero width/height)\n");
        return 1;
    }

    std::fprintf(stderr, "generated %llu points, accepted %llu (%.1f%%)\n",
                 static_cast<unsigned long long>(image.stats.pointsGenerated),
                 static_cast<unsigned long long>(image.stats.pointsAccepted),
                 image.stats.pointsGenerated > 0
                     ? 100.0 * static_cast<double>(image.stats.pointsAccepted) / static_cast<double>(image.stats.pointsGenerated)
                     : 0.0);

    if (!apo::writePng(outputPath, image.width, image.height, image.channels, image.pixels.data())) {
        std::fprintf(stderr, "error: failed to write PNG to '%s'\n", outputPath.c_str());
        return 1;
    }

    std::fprintf(stderr, "wrote %s\n", outputPath.c_str());
    return 0;
}
