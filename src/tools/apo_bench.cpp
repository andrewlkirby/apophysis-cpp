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
//                   [--backend=cpu|gpu|hybrid|both|<comma list>]
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
//                  "1,0" - single-threaded baseline, then auto. Ignored for
//                  gpu rows (the GPU backend has its own internal launch-
//                  chunk parallelism, not a CPU-style thread count).
//   --flame=...    which fixture(s) to run (default all)
//   --bucket-precision=double|float  BucketPrecision to render with
//                  (default double, matching every real caller's own
//                  default - see Renderer.h's B5 doc comment)
//   --backend=...  cpu (default, unchanged prior behavior): the threadCount
//                  loop only. gpu: one GPU row per fixture instead (via
//                  RenderDispatcher with preferGpu forced on - reports
//                  "GPU not available" per fixture instead of silently
//                  timing a CPU fallback if this build/machine can't
//                  actually run that fixture on the GPU, e.g. no
//                  APO_ENABLE_CUDA, no CUDA device, or - not applicable to
//                  any of this file's own fixtures, which use only ported
//                  variations - a plugin-using flame). both: cpu rows then
//                  one gpu row per fixture, for direct throughput
//                  comparison - see docs/GPU_RENDERING_PLAN.md. GPU rows
//                  are skipped (with the reason) for renders below
//                  RenderDispatcher::kMinGpuBatches sub-batches, since
//                  real dispatch keeps those on the CPU. hybrid: one row
//                  per fixture rendered GPU + (hardware_concurrency - 1)
//                  CPU threads (docs/HYBRID_CPU_GPU_RENDERING_PLAN.md).
//                  A comma list runs several, e.g. --backend=gpu,hybrid.
//                  probe: run BackendProbe (the Render dialog's Auto
//                  backend) and print its rates, pick and time estimates.
//                  Hybrid rows honour --threads: N = GPU + (N - 1) CPU
//                  threads, 0 = auto.
//   --gpu-kernel=auto|thread|warp  chaos kernel for gpu/hybrid rows:
//                  per-thread xform choice, warp-uniform xform choice with
//                  point shuffling, or the renderer's own pick (default).
//   --file=PATH    bench the first flame in a .flame file instead of the
//                  fixtures; it keeps its own size/density/oversample
//                  unless --size/--density/--oversample are given.

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/Flame.h"
#include "core/VariationRegistry.h"
#include "core/io/FlameIO.h"
#include "core/render/BackendProbe.h"
#include "core/render/RenderDispatcher.h"
#include "core/render/Renderer.h"

#ifdef APO_HAVE_CUDA
#include "core/render/gpu/GpuRenderer.h"
#endif

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

std::vector<std::string> splitComma(const std::string& s) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t comma = s.find(',', pos);
        std::string tok = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        if (!tok.empty()) out.push_back(std::move(tok));
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out;
}

std::vector<int> parseThreadList(const std::string& s) {
    std::vector<int> out;
    for (const std::string& tok : splitComma(s)) out.push_back(std::atoi(tok.c_str()));
    return out;
}

// useGpu=false (default path, unchanged): renders via Renderer::render()
// directly at the given CPU threadCount. useGpu=true: renders via
// RenderDispatcher with preferGpu forced on, ignoring threadCount entirely
// (the GPU backend has its own internal parallelism - see
// docs/GPU_RENDERING_PLAN.md's chaos_kernel.cu mapping comment) - first
// checks RenderDispatcher::wouldUseGpu() and prints a clear "not available"
// line instead of silently benchmarking a CPU fallback mislabeled as GPU
// (e.g. a non-APO_ENABLE_CUDA build, or no CUDA device on this machine).
enum class Backend { Cpu, Gpu, Hybrid };

const char* backendName(apo::RenderBackend b) {
    switch (b) {
        case apo::RenderBackend::Cpu: return "cpu";
        case apo::RenderBackend::Gpu: return "gpu";
        case apo::RenderBackend::Hybrid: return "gpu+cpu";
        case apo::RenderBackend::Auto: break;
    }
    return "auto";
}

void runProbe(const char* name, const apo::Flame& flame, apo::BucketPrecision precision) {
    const auto t0 = std::chrono::steady_clock::now();
    const apo::BackendProbeResult r = apo::probeBackends(flame, 0, precision, /*preferGpu=*/true);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (!r.valid) {
        std::printf("%-16s probe failed\n", name);
        return;
    }
    const std::uint64_t total = apo::plannedPointCount(flame);
    std::printf("%-16s probe (%.2fs): gpu %.2fM pts/s, cpu(%d threads) %.2fM pts/s, ratio %.2f -> %s\n", name,
                secs, r.gpuPointsPerSec / 1e6, r.cpuThreads, r.cpuPointsPerSec / 1e6,
                r.gpuPointsPerSec > 0 ? r.cpuPointsPerSec / r.gpuPointsPerSec : 0.0,
                backendName(apo::recommendBackend(r)));
    std::printf("%-16s   estimates: cpu %.1fs, gpu %.1fs, gpu+cpu %.1fs\n", "",
                apo::estimateRenderSeconds(r, apo::RenderBackend::Cpu, total),
                apo::estimateRenderSeconds(r, apo::RenderBackend::Gpu, total),
                apo::estimateRenderSeconds(r, apo::RenderBackend::Hybrid, total));
}

// --gpu-kernel: 0 = auto, 1 = per-thread, 2 = warp-uniform.
int gGpuKernel = 0;

apo::RenderedImage renderGpu(const apo::Flame& flame, std::uint64_t seed, int threadCount, apo::RenderTimings* timings,
                             apo::BucketPrecision precision, bool hybrid) {
#ifdef APO_HAVE_CUDA
    if (gGpuKernel != 0) {
        return apo::gpu::GpuRenderer::render(
            flame, seed, nullptr, timings, precision,
            hybrid ? apo::RenderDispatcher::cpuAssistThreadCount(threadCount) : 0,
            gGpuKernel == 1 ? apo::gpu::XformScheduling::PerThread : apo::gpu::XformScheduling::WarpUniform);
    }
#endif
    return apo::RenderDispatcher::render(flame, seed, hybrid ? threadCount : 0, /*progress=*/nullptr, timings,
                                         precision, /*preferGpu=*/true, /*usedGpu=*/nullptr, /*cpuAssist=*/hybrid);
}

const char* kernelDesc(const apo::Flame& flame) {
#ifdef APO_HAVE_CUDA
    const auto requested = gGpuKernel == 1   ? apo::gpu::XformScheduling::PerThread
                           : gGpuKernel == 2 ? apo::gpu::XformScheduling::WarpUniform
                                             : apo::gpu::XformScheduling::Auto;
    return apo::gpu::GpuRenderer::resolveScheduling(flame, requested) == apo::gpu::XformScheduling::WarpUniform
               ? "warp"
               : "thread";
#else
    (void)flame;
    return "-";
#endif
}

void runOne(const char* fixtureName, const apo::Flame& flame, std::uint64_t seed, int threadCount,
            apo::BucketPrecision precision, Backend backend) {
    const bool useGpu = backend != Backend::Cpu;
    const bool hybrid = backend == Backend::Hybrid;
    if (useGpu && !apo::RenderDispatcher::wouldUseGpu(flame, /*preferGpu=*/true)) {
        // Includes "render too small to benefit from GPU" (see
        // RenderDispatcher::kMinGpuBatches) - the bench reports what real
        // dispatch would do rather than timing a path no caller can reach.
        std::printf("%-16s threads=%-11s prec=%-6s  GPU not used: %s - skipped\n", fixtureName,
                    hybrid ? "gpu+cpu" : "gpu",
                    precision == apo::BucketPrecision::Float ? "float" : "double",
                    apo::RenderDispatcher::cpuFallbackReason(flame, /*preferGpu=*/true).c_str());
        return;
    }

    if (hybrid && apo::RenderDispatcher::cpuAssistThreadCount(threadCount) == 0) {
        std::printf("%-16s threads=%-11s skipped: --threads=%d leaves no CPU assist threads\n", fixtureName,
                    "gpu+cpu", threadCount);
        return;
    }

    apo::RenderTimings timings;
    const auto wallStart = std::chrono::steady_clock::now();
    apo::RenderedImage image =
        useGpu ? renderGpu(flame, seed, threadCount, &timings, precision, hybrid)
               : apo::Renderer::render(flame, seed, threadCount, /*progress=*/nullptr, &timings, precision);
    const double wallSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();

    const double totalSeconds =
        timings.setupSeconds + timings.iterateSeconds + timings.reduceSeconds + timings.tonemapSeconds;
    const double pointsPerSec =
        totalSeconds > 0 ? static_cast<double>(image.stats.pointsGenerated) / totalSeconds : 0.0;

    const std::string threadsDesc =
        hybrid ? "gpu+" + std::to_string(apo::RenderDispatcher::cpuAssistThreadCount(threadCount)) + "cpu" : useGpu ? "gpu" : (threadCount == 1 ? "1 (single)" : (threadCount == 0 ? "auto" : std::to_string(threadCount)));
    const char* precisionDesc = precision == apo::BucketPrecision::Float ? "float" : "double";
    std::printf(
        "%-16s threads=%-11s prec=%-6s points=%12llu  %8.2fM pts/sec  wall=%7.3fs  "
        "[setup=%6.3fs iterate=%6.3fs reduce=%6.3fs tonemap=%6.3fs]\n",
        fixtureName, threadsDesc.c_str(), precisionDesc, static_cast<unsigned long long>(image.stats.pointsGenerated),
        pointsPerSec / 1e6, wallSeconds, timings.setupSeconds, timings.iterateSeconds, timings.reduceSeconds,
        timings.tonemapSeconds);
    if (useGpu) std::printf("%-16s   gpu kernel: %s\n", "", kernelDesc(flame));
    if (hybrid && image.stats.pointsGenerated > 0) {
        std::printf("%-16s   cpu share of points: %.1f%%\n", "",
                    100.0 * static_cast<double>(image.stats.cpuAssistPointsGenerated) /
                        static_cast<double>(image.stats.pointsGenerated));
    }
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
    std::string backendArg = "cpu";
    std::string fileArg;
    bool sizeSet = false, densitySet = false, oversampleSet = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        std::string value;
        if (parseOption(arg, "density", value)) {
            density = std::atof(value.c_str());
            densitySet = true;
        } else if (parseOption(arg, "oversample", value)) {
            oversample = std::atoi(value.c_str());
            oversampleSet = true;
        } else if (parseOption(arg, "size", value)) {
            const size_t x = value.find('x');
            if (x == std::string::npos) {
                std::fprintf(stderr, "error: --size expects WxH, e.g. --size=800x600\n");
                return 2;
            }
            width = std::atoi(value.substr(0, x).c_str());
            height = std::atoi(value.substr(x + 1).c_str());
            sizeSet = true;
        } else if (parseOption(arg, "seed", value)) {
            seed = static_cast<std::uint64_t>(std::strtoull(value.c_str(), nullptr, 10));
        } else if (parseOption(arg, "threads", value)) {
            threadsArg = value;
        } else if (parseOption(arg, "flame", value)) {
            flameArg = value;
        } else if (parseOption(arg, "gpu-kernel", value)) {
            if (value == "auto") gGpuKernel = 0;
            else if (value == "thread") gGpuKernel = 1;
            else if (value == "warp") gGpuKernel = 2;
            else {
            std::fprintf(stderr, "error: --gpu-kernel must be auto, thread or warp\n");
                return 2;
            }
        } else if (parseOption(arg, "file", value)) {
            fileArg = value;
        } else if (parseOption(arg, "bucket-precision", value)) {
            precisionArg = value;
        } else if (parseOption(arg, "backend", value)) {
            backendArg = value;
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

    bool runCpu = false, runGpu = false, runHybrid = false, runProbeRow = false, backendOk = true;
    for (const std::string& token : splitComma(backendArg)) {
        if (token == "cpu") runCpu = true;
        else if (token == "gpu") runGpu = true;
        else if (token == "hybrid") runHybrid = true;
        else if (token == "probe") runProbeRow = true;
        else if (token == "both") runCpu = runGpu = true;
        else backendOk = false;
    }
    if (!backendOk || !(runCpu || runGpu || runHybrid || runProbeRow)) {
        std::fprintf(stderr, "error: --backend='%s' must be a comma list of cpu, gpu, hybrid, both\n",
                     backendArg.c_str());
        return 2;
    }

    const std::vector<int> threadCounts = parseThreadList(threadsArg);
    if (threadCounts.empty()) {
        std::fprintf(stderr, "error: --threads produced an empty list\n");
        return 2;
    }

    if (!fileArg.empty()) {
        auto flames = apo::loadFlameFile(fileArg);
        if (flames.empty()) {
            std::fprintf(stderr, "error: --file='%s' has no readable flame\n", fileArg.c_str());
            return 2;
        }
        apo::Flame& flame = *flames.front();
        if (sizeSet) {
            flame.adjustScale(width, height);
        }
        if (densitySet) flame.sampleDensity = density;
        if (oversampleSet) flame.spatialOversample = oversample;
        std::printf("apo_bench: file=%s size=%dx%d density=%.1f oversample=%d seed=%llu bucket-precision=%s "
                    "backend=%s hardware_concurrency=%u\n",
                    fileArg.c_str(), flame.width, flame.height, flame.sampleDensity, flame.spatialOversample,
                    static_cast<unsigned long long>(seed), precisionArg.c_str(), backendArg.c_str(),
                    std::thread::hardware_concurrency());
        // Backends in the order given, so e.g. gpu,hybrid,hybrid,gpu runs ABBA.
        for (const std::string& token : splitComma(backendArg)) {
            if (token == "cpu" || token == "both") {
                for (int t : threadCounts) runOne("file", flame, seed, t, precision, Backend::Cpu);
            }
            if (token == "gpu" || token == "both") runOne("file", flame, seed, 0, precision, Backend::Gpu);
            if (token == "hybrid") {
                for (int t : threadCounts) runOne("file", flame, seed, t, precision, Backend::Hybrid);
            }
            if (token == "probe") runProbe("file", flame, precision);
            std::fflush(stdout);
        }
        return 0;
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

    std::printf("apo_bench: size=%dx%d density=%.1f oversample=%d seed=%llu bucket-precision=%s backend=%s "
                "hardware_concurrency=%u\n",
                width, height, density, oversample, static_cast<unsigned long long>(seed), precisionArg.c_str(),
                backendArg.c_str(), std::thread::hardware_concurrency());
    std::printf("--------------------------------------------------------------------------------------------------------------\n");

    for (const Fixture* f : fixtures) {
        auto flame = f->build(width, height, density);
        flame->spatialOversample = oversample;
        if (runCpu) {
            for (int threadCount : threadCounts) {
                runOne(f->name, *flame, seed, threadCount, precision, Backend::Cpu);
            }
        }
        if (runGpu) {
            runOne(f->name, *flame, seed, /*threadCount=*/0, precision, Backend::Gpu);
        }
        if (runProbeRow) runProbe(f->name, *flame, precision);
        if (runHybrid) {
            for (int threadCount : threadCounts) {
                runOne(f->name, *flame, seed, threadCount, precision, Backend::Hybrid);
            }
        }
    }

    return 0;
}
