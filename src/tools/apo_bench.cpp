// apo_bench - B0's points-per-second benchmark harness (FOLLOWUP_PLAN.txt
// Part B). A manually-run tool, not a ctest (see tests/CMakeLists.txt's own
// comment on why: this is meant to be run deliberately, before/after a
// specific performance change, not on every build). Renders a small set of
// representative flames - a pure-affine linear-only IFS, a
// transcendental-heavy one (julian+swirl, exercising sin/cos/atan2/pow-heavy
// variation math), and a blur-heavy one (gaussian_blur, exercising the
// stochastic/extra-RNG-draw variation path) - at a fixed seed/size/density,
// both single-threaded (deterministic baseline) and with the default
// hardware_concurrency() thread count, and reports points/sec plus
// Renderer::RenderTimings' per-phase wall-clock breakdown (setup / iterate /
// reduce / tonemap) for each. Every item in Part B's plan should get a
// measured before/after here, recorded in FOLLOWUP_PLAN.txt's progress log -
// nothing should land on faith.
//
// Usage: apo_bench [--density=N] [--size=WxH] [--oversample=N] [--seed=N]
//                   [--threads=1,0,4] [--flame=all|linear|transcendental|blur]
//   --density=N    flame.sampleDensity for every fixture (default 200)
//   --size=WxH     flame.width x flame.height for every fixture (default
//                  800x600)
//   --oversample=N flame.spatialOversample for every fixture (default 1) -
//                  raising this is the quickest way to grow bucket area
//                  (hence reduce/tonemap cost) independent of density, e.g.
//                  for B4/B5-style reduce/tonemap-focused measurements
//   --seed=N       RNG seed (default 1)
//   --threads=...  comma-separated threadCount list to pass to
//                  Renderer::render (0 = hardware_concurrency); default
//                  "1,0" - single-threaded baseline, then auto
//   --flame=...    which fixture(s) to run (default all)
//   --bucket-precision=double|float  Renderer::BucketPrecision to render
//                  with (default double, matching every real caller's own
//                  default - see Renderer.h's B5 doc comment)

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/Flame.h"
#include "core/VariationRegistry.h"
#include "core/render/Renderer.h"

namespace {

void setGrayscaleColorMap(apo::Flame& flame) {
    for (int i = 0; i < 256; ++i) {
        flame.cmap.entries[i][0] = static_cast<std::uint8_t>(i);
        flame.cmap.entries[i][1] = static_cast<std::uint8_t>(i);
        flame.cmap.entries[i][2] = static_cast<std::uint8_t>(i);
        flame.cmap.entries[i][3] = 255;
    }
}

int registeredIndex(const std::string& name) {
    return apo::VariationRegistry::instance().variationIndex(name);
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

// Pure affine, no variations at all (vars_[0]="linear" stays at its
// XForm::clear() default weight of 1, everything else 0) - the classic
// 3-map Sierpinski-gasket IFS, same shape as renderer_test.cpp's own
// makeSierpinskiFlame(). Exercises bucket accumulation and xform-selection
// dispatch with the cheapest possible per-point variation cost, so it's the
// harness's floor: any dispatch-overhead win (B2) should show up here most
// clearly, since dispatch is a bigger fraction of a linear point's total
// cost than a transcendental one's.
std::unique_ptr<apo::Flame> makeLinearFlame(int width, int height, double density) {
    auto flame = baseFlame(width, height, density);
    const double corners[3][2] = {{-0.5, -0.5}, {0.5, -0.5}, {0.0, 0.5}};
    for (int i = 0; i < 3; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.color = i / 2.0;
        xf.c[0] = {0.5, 0.0};
        xf.c[1] = {0.0, 0.5};
        xf.c[2] = {corners[i][0], corners[i][1]};
    }
    return flame;
}

// julian (registered variation - sin/cos/atan2/pow per point) blended with
// swirl (local variation - sin/cos per point) across 3 xforms - the
// harness's transcendental-math ceiling.
std::unique_ptr<apo::Flame> makeTranscendentalFlame(int width, int height, double density) {
    auto flame = baseFlame(width, height, density);
    const int julianIdx = registeredIndex("julian");
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
        xf.c[0] = {coefs[i][0], coefs[i][3]};
        xf.c[1] = {coefs[i][1], coefs[i][4]};
        xf.c[2] = {coefs[i][2], coefs[i][5]};
        xf.setVariation(0, 0.0); // turn off linear's default weight-1
        xf.setVariation(registeredIndex("swirl"), 0.6);
        xf.setVariation(julianIdx, 1.0);
    }
    return flame;
}

// gaussian_blur (2 extra rng.uniform01() draws + sin/cos per point) across 3
// xforms - the harness's stochastic-variation ceiling, representative of
// blur-family flames' extra RNG traffic.
std::unique_ptr<apo::Flame> makeBlurFlame(int width, int height, double density) {
    auto flame = baseFlame(width, height, density);
    const double corners[3][2] = {{-0.5, -0.5}, {0.5, -0.5}, {0.0, 0.5}};
    const int blurIdx = registeredIndex("gaussian_blur");
    for (int i = 0; i < 3; ++i) {
        apo::XForm& xf = *flame->xform[i];
        xf.clear();
        xf.density = 1.0;
        xf.color = i / 2.0;
        xf.c[0] = {0.5, 0.0};
        xf.c[1] = {0.0, 0.5};
        xf.c[2] = {corners[i][0], corners[i][1]};
        xf.setVariation(0, 0.3);
        xf.setVariation(blurIdx, 0.7);
    }
    return flame;
}

struct Fixture {
    const char* name;
    std::unique_ptr<apo::Flame> (*build)(int, int, double);
};

bool parseOption(const std::string& arg, const char* name, std::string& outValue) {
    const std::string prefix = std::string("--") + name + "=";
    if (arg.rfind(prefix, 0) != 0) return false;
    outValue = arg.substr(prefix.size());
    return true;
}

std::vector<int> parseThreadList(const std::string& s) {
    std::vector<int> out;
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t comma = s.find(',', pos);
        const std::string tok = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        if (!tok.empty()) out.push_back(std::atoi(tok.c_str()));
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out;
}

void runOne(const char* fixtureName, const apo::Flame& flame, std::uint64_t seed, int threadCount,
            apo::BucketPrecision precision) {
    apo::RenderTimings timings;
    const auto wallStart = std::chrono::steady_clock::now();
    apo::RenderedImage image =
        apo::Renderer::render(flame, seed, threadCount, /*progress=*/nullptr, &timings, precision);
    const double wallSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();

    const double totalSeconds =
        timings.setupSeconds + timings.iterateSeconds + timings.reduceSeconds + timings.tonemapSeconds;
    const double pointsPerSec =
        totalSeconds > 0 ? static_cast<double>(image.stats.pointsGenerated) / totalSeconds : 0.0;

    const std::string threadsDesc =
        threadCount == 1 ? "1 (single)" : (threadCount == 0 ? "auto" : std::to_string(threadCount));
    const char* precisionDesc = precision == apo::BucketPrecision::Float ? "float" : "double";
    std::printf(
        "%-16s threads=%-11s prec=%-6s points=%12llu  %8.2fM pts/sec  wall=%7.3fs  "
        "[setup=%6.3fs iterate=%6.3fs reduce=%6.3fs tonemap=%6.3fs]\n",
        fixtureName, threadsDesc.c_str(), precisionDesc, static_cast<unsigned long long>(image.stats.pointsGenerated),
        pointsPerSec / 1e6, wallSeconds, timings.setupSeconds, timings.iterateSeconds, timings.reduceSeconds,
        timings.tonemapSeconds);
}

} // namespace

int main(int argc, char** argv) {
    int width = 800, height = 600;
    double density = 200.0;
    int oversample = 1;
    std::uint64_t seed = 1;
    std::string threadsArg = "1,0";
    std::string flameArg = "all";
    std::string precisionArg = "double";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        std::string value;
        if (parseOption(arg, "density", value)) {
            density = std::atof(value.c_str());
        } else if (parseOption(arg, "oversample", value)) {
            oversample = std::atoi(value.c_str());
        } else if (parseOption(arg, "size", value)) {
            const size_t x = value.find('x');
            if (x == std::string::npos) {
                std::fprintf(stderr, "error: --size expects WxH, e.g. --size=800x600\n");
                return 2;
            }
            width = std::atoi(value.substr(0, x).c_str());
            height = std::atoi(value.substr(x + 1).c_str());
        } else if (parseOption(arg, "seed", value)) {
            seed = static_cast<std::uint64_t>(std::strtoull(value.c_str(), nullptr, 10));
        } else if (parseOption(arg, "threads", value)) {
            threadsArg = value;
        } else if (parseOption(arg, "flame", value)) {
            flameArg = value;
        } else if (parseOption(arg, "bucket-precision", value)) {
            precisionArg = value;
        } else {
            std::fprintf(stderr, "unrecognized option: %s\n", arg.c_str());
            return 2;
        }
    }

    apo::BucketPrecision precision;
    if (precisionArg == "double") {
        precision = apo::BucketPrecision::Double;
    } else if (precisionArg == "float") {
        precision = apo::BucketPrecision::Float;
    } else {
        std::fprintf(stderr, "error: --bucket-precision='%s' must be 'double' or 'float'\n", precisionArg.c_str());
        return 2;
    }

    const std::vector<int> threadCounts = parseThreadList(threadsArg);
    if (threadCounts.empty()) {
        std::fprintf(stderr, "error: --threads produced an empty list\n");
        return 2;
    }

    const std::vector<Fixture> allFixtures = {
        {"linear", &makeLinearFlame},
        {"transcendental", &makeTranscendentalFlame},
        {"blur", &makeBlurFlame},
    };

    std::vector<Fixture const*> fixtures;
    for (const auto& f : allFixtures) {
        if (flameArg == "all" || flameArg == f.name) fixtures.push_back(&f);
    }
    if (fixtures.empty()) {
        std::fprintf(stderr, "error: --flame='%s' matches no known fixture (linear/transcendental/blur/all)\n",
                     flameArg.c_str());
        return 2;
    }

    std::printf("apo_bench: size=%dx%d density=%.1f oversample=%d seed=%llu bucket-precision=%s "
                "hardware_concurrency=%u\n",
                width, height, density, oversample, static_cast<unsigned long long>(seed), precisionArg.c_str(),
                std::thread::hardware_concurrency());
    std::printf("--------------------------------------------------------------------------------------------------------------\n");

    for (const Fixture* f : fixtures) {
        auto flame = f->build(width, height, density);
        flame->spatialOversample = oversample;
        for (int threadCount : threadCounts) {
            runOne(f->name, *flame, seed, threadCount, precision);
        }
    }

    return 0;
}
