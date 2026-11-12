// End-to-end save fidelity: a flame edited the way the editor/adjust/
// post-process/render dialogs edit it must come back from a save -> load
// round trip with every user-visible setting *exactly* equal (field by
// field, not just "re-saves to the same XML", which can't notice a field
// the writer never writes at all), and must render the same.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/Rng.h"
#include "core/Variation.h"
#include "core/VariationRegistry.h"
#include "core/XForm.h"
#include "core/edit/RandomFlame.h"
#include "core/io/FlameIO.h"
#include "core/render/Renderer.h"

using apo_test::check;

namespace {

// An "awkward" value: never a round decimal, so any precision loss shows.
double odd(apo::Rng& rng, double lo, double hi) { return lo + (hi - lo) * rng.uniform01() + 1.234567890123e-7; }

void simulateUserEdits(apo::Flame& flame, std::uint64_t seed) {
    apo::MtRng rng(seed);
    const int n = flame.numXForms();
    for (int i = 0; i < n; ++i) {
        apo::XForm& xf = *flame.xform[i];
        // Triangle/transform panel.
        xf.rotate(odd(rng, 0, 37));
        xf.scale(odd(rng, 0.9, 1.05));
        xf.translate(odd(rng, -0.02, 0.02), odd(rng, -0.02, 0.02));
        xf.p = {{{odd(rng, 0.9, 1.1), odd(rng, -0.1, 0.1)}, {odd(rng, -0.1, 0.1), odd(rng, 0.9, 1.1)},
                 {odd(rng, -0.01, 0.01), odd(rng, -0.01, 0.01)}}};
        xf.density = odd(rng, 0.1, 2);
        xf.color = odd(rng, 0, 1);
        xf.symmetry = odd(rng, -1, 1);
        xf.transOpacity = odd(rng, 0.5, 1);
        xf.pluginColor = odd(rng, 0, 1);
        xf.transformName = "xf " + std::to_string(i) + " <&\"'>";
        // Xaos.
        for (int j = 0; j < n; ++j) xf.modWeights[j] = (i + j) % 3 == 0 ? 0.0 : odd(rng, 0.1, 3);
    }
    // Final xform.
    flame.finalXformEnabled = flame.useFinalXform = true;
    apo::XForm& fx = *flame.finalXform;
    fx.rotate(odd(rng, 0, 10));
    // Transform panel's color tab on the final xform (seed-dependent so
    // both the untouched defaults and edited values get covered).
    if (seed % 3 == 0) {
        fx.color = odd(rng, 0, 1);
        fx.symmetry = odd(rng, -1, 1);
        fx.transOpacity = odd(rng, 0, 1);
    }
    fx.setVariation(0, odd(rng, 0.5, 1));
    fx.setVariation(apo::VariationRegistry::instance().variationIndex("julian"), odd(rng, 0.1, 0.4));
    double jp = 3, jd = odd(rng, 0.5, 2);
    fx.setVariable("julian_power", jp);
    fx.setVariable("julian_dist", jd);

    // Camera / adjust panel.
    flame.center = {flame.center[0] + odd(rng, -1e-3, 1e-3), flame.center[1] + odd(rng, -1e-3, 1e-3)};
    flame.zoom = odd(rng, -0.5, 0.5);
    flame.angle = odd(rng, -1, 1);
    flame.cameraYaw = odd(rng, -1, 1);
    flame.cameraPitch = odd(rng, -1, 1);
    flame.cameraPersp = seed % 2 ? 0.0 : odd(rng, 0, 1);
    flame.cameraZpos = odd(rng, -0.2, 0.2);
    flame.cameraDOF = odd(rng, 0, 0.05);
    // Tone / post-process.
    flame.gamma = odd(rng, 1, 5);
    flame.brightness = odd(rng, 1, 5);
    flame.vibrancy = odd(rng, 0, 1);
    flame.contrast = odd(rng, 0.5, 2);
    flame.gammaThreshold = odd(rng, 0, 0.1);
    flame.background = {17, 33, 99, 0};
    flame.curves[0].points[1] = {odd(rng, 0, 0.5), odd(rng, 0, 0.5)};
    flame.curves[2].weights[2] = odd(rng, 0.5, 2);
    // Render dialog.
    flame.transparency = true;
    flame.spatialOversample = 2;
    flame.spatialFilterRadius = odd(rng, 0.2, 1);
    flame.sampleDensity = odd(rng, 3, 6);
    flame.enableDE = true;
    flame.estimator = odd(rng, 1, 9);
    flame.estimatorMin = odd(rng, 0, 1);
    flame.estimatorCurve = odd(rng, 0.1, 0.9);
    flame.adjustScale(333, 211);
    flame.name = "Edited & <saved> \"flame\"";
    // Gradient editor.
    for (int i = 0; i < 256; ++i) flame.cmap.entries[i] = {i, 255 - i, (i * 7) % 256, 255};
}

int fieldMismatches = 0;

void expectEq(bool equal, const std::string& what) {
    if (!equal) {
        std::fprintf(stderr, "  mismatch: %s\n", what.c_str());
        ++fieldMismatches;
    }
}

#define EXPECT_FIELD(a, b, field) expectEq((a).field == (b).field, prefix + #field)

void compareXForm(const apo::XForm& a, const apo::XForm& b, const std::string& prefix, bool isFinal) {
    EXPECT_FIELD(a, b, c);
    EXPECT_FIELD(a, b, p);
    EXPECT_FIELD(a, b, color);
    EXPECT_FIELD(a, b, pluginColor);
    EXPECT_FIELD(a, b, symmetry);
    EXPECT_FIELD(a, b, transOpacity);
    if (!isFinal) {
        EXPECT_FIELD(a, b, density);
        EXPECT_FIELD(a, b, transformName);
        EXPECT_FIELD(a, b, modWeights);
    }
    auto& reg = apo::VariationRegistry::instance();
    for (int v = 0; v < a.numVariations(); ++v) {
        expectEq(a.variation(v) == b.variation(v), prefix + "weight " + reg.varName(v));
        if (a.variation(v) == 0 || v < apo::VariationRegistry::kNumLocalVars) continue;
        const auto& factory = reg.registeredVariation(v - apo::VariationRegistry::kNumLocalVars);
        for (int j = 0; j < factory.numVariables(); ++j) {
            const std::string name = factory.variableNameAt(j);
            double va = 0, vb = 0;
            const bool ha = a.getVariable(name, va), hb = b.getVariable(name, vb);
            expectEq(ha == hb && va == vb, prefix + "param " + name);
        }
    }
}

void compareFlames(const apo::Flame& a, const apo::Flame& b) {
    std::string prefix;
    EXPECT_FIELD(a, b, name);
    EXPECT_FIELD(a, b, width);
    EXPECT_FIELD(a, b, height);
    EXPECT_FIELD(a, b, center);
    EXPECT_FIELD(a, b, pixelsPerUnit);
    EXPECT_FIELD(a, b, zoom);
    EXPECT_FIELD(a, b, angle);
    EXPECT_FIELD(a, b, cameraPitch);
    EXPECT_FIELD(a, b, cameraYaw);
    EXPECT_FIELD(a, b, cameraPersp);
    EXPECT_FIELD(a, b, cameraZpos);
    EXPECT_FIELD(a, b, cameraDOF);
    EXPECT_FIELD(a, b, gamma);
    EXPECT_FIELD(a, b, brightness);
    EXPECT_FIELD(a, b, vibrancy);
    EXPECT_FIELD(a, b, contrast);
    EXPECT_FIELD(a, b, gammaThreshold);
    expectEq(a.background[0] == b.background[0] && a.background[1] == b.background[1] &&
                 a.background[2] == b.background[2],
             "background");
    EXPECT_FIELD(a, b, curves);
    EXPECT_FIELD(a, b, transparency);
    EXPECT_FIELD(a, b, spatialOversample);
    EXPECT_FIELD(a, b, spatialFilterRadius);
    EXPECT_FIELD(a, b, sampleDensity);
    EXPECT_FIELD(a, b, enableDE);
    EXPECT_FIELD(a, b, estimator);
    EXPECT_FIELD(a, b, estimatorMin);
    EXPECT_FIELD(a, b, estimatorCurve);
    EXPECT_FIELD(a, b, soloXform);
    EXPECT_FIELD(a, b, time);
    EXPECT_FIELD(a, b, finalXformEnabled);
    EXPECT_FIELD(a, b, useFinalXform);
    for (int i = 0; i < 256; ++i) {
        for (int ch = 0; ch < 3; ++ch) expectEq(a.cmap.entries[i][ch] == b.cmap.entries[i][ch], "gradient");
    }
    expectEq(a.numXForms() == b.numXForms(), "numXForms");
    for (int i = 0; i < std::min(a.numXForms(), b.numXForms()); ++i) {
        compareXForm(*a.xform[i], *b.xform[i], "xform " + std::to_string(i) + " ", false);
    }
    if (a.finalXformEnabled) compareXForm(*a.finalXform, *b.finalXform, "final ", true);
}

double meanAbsDiff(const apo::RenderedImage& a, const apo::RenderedImage& b) {
    if (a.pixels.size() != b.pixels.size() || a.pixels.empty()) return 255.0;
    double sum = 0;
    for (size_t i = 0; i < a.pixels.size(); ++i) sum += std::abs(int(a.pixels[i]) - int(b.pixels[i]));
    return sum / static_cast<double>(a.pixels.size());
}

std::unique_ptr<apo::Flame> roundTrip(const apo::Flame& flame) {
    auto loaded = apo::loadFlameFromString(apo::saveFlameToString({&flame}));
    return loaded.size() == 1 ? std::move(loaded[0]) : nullptr;
}

// A parameter name owned by two variation types (or shadowing a variation
// name) can't be saved/loaded unambiguously - the loader routes it to the
// first owner only.
void testParameterNamesAreUnambiguous() {
    auto& reg = apo::VariationRegistry::instance();
    std::set<std::string> seen;
    int problems = 0;
    for (int v = 0; v < reg.numRegisteredVariations(); ++v) {
        const auto& f = reg.registeredVariation(v);
        for (int j = 0; j < f.numVariables(); ++j) {
            const std::string name = f.variableNameAt(j);
            if (!seen.insert(name).second || reg.variationIndex(name) >= 0) {
                std::fprintf(stderr, "  ambiguous parameter name '%s' (in %s)\n", name.c_str(), f.name().c_str());
                ++problems;
            }
        }
    }
    check(problems == 0, "every variation parameter name is unique and never a variation name");
}

// Every variation type, with every parameter set to an awkward value, must
// reload with the same weight and parameters.
void testEveryVariationRoundTrips() {
    auto& reg = apo::VariationRegistry::instance();
    apo::MtRng rng(99);
    int failed = 0;
    for (int v = 0; v < reg.nrVar(); ++v) {
        apo::Flame flame;
        flame.clear();
        flame.xform[0]->density = 1;
        flame.xform[0]->setVariation(0, 0);
        flame.xform[0]->setVariation(v, odd(rng, 0.2, 1));
        if (v >= apo::VariationRegistry::kNumLocalVars) {
            const auto& f = reg.registeredVariation(v - apo::VariationRegistry::kNumLocalVars);
            for (int j = 0; j < f.numVariables(); ++j) {
                double cur = 0;
                flame.xform[0]->getVariable(f.variableNameAt(j), cur);
                double val = cur + odd(rng, 0.1, 0.9);
                flame.xform[0]->setVariable(f.variableNameAt(j), val);
            }
        }
        auto loaded = roundTrip(flame);
        const int before = fieldMismatches;
        if (loaded) compareXForm(*flame.xform[0], *loaded->xform[0], reg.varName(v) + ": ", false);
        if (!loaded || fieldMismatches != before) ++failed;
    }
    fieldMismatches = 0;
    check(failed == 0, "every variation's weight and parameters round-trip exactly");
}

void testEditedRandomFlames() {
    std::vector<int> eligible;
    for (int i = 0; i < apo::VariationRegistry::instance().nrVar(); ++i) eligible.push_back(i);

    for (std::uint64_t seed = 1; seed <= 12; ++seed) {
        auto flame = apo::generateRandomFlame(seed * 7919, 320, 200, 2, 6, -1, &eligible,
                                              apo::RandomGradientSource::Calculated, nullptr, 1, 5, 0.1, 1.0, true,
                                              1.0);
        if (!flame) continue;
        simulateUserEdits(*flame, seed);
        if (seed == 3) flame->soloXform = 1;

        auto loaded = roundTrip(*flame);
        char desc[128];
        std::snprintf(desc, sizeof(desc), "seed %llu: reloads", static_cast<unsigned long long>(seed));
        if (!check(loaded != nullptr, desc)) continue;

        fieldMismatches = 0;
        compareFlames(*flame, *loaded);
        std::snprintf(desc, sizeof(desc), "seed %llu: every setting reloads exactly",
                      static_cast<unsigned long long>(seed));
        check(fieldMismatches == 0, desc);

        // Save -> load -> save must also be a fixed point.
        std::snprintf(desc, sizeof(desc), "seed %llu: re-save is byte-identical",
                      static_cast<unsigned long long>(seed));
        check(apo::saveFlameToString({flame.get()}) == apo::saveFlameToString({loaded.get()}), desc);

        // The renderer isn't bit-reproducible for every variation (some keep
        // state between renders), so the reloaded flame only has to be no
        // further from the original than a plain re-render of it is.
        flame->sampleDensity = loaded->sampleDensity = 3;
        const auto before = apo::Renderer::render(*flame, 42, 1);
        const auto after = apo::Renderer::render(*loaded, 42, 1);
        const auto again = apo::Renderer::render(*flame, 42, 1);
        const double noise = meanAbsDiff(before, again), drift = meanAbsDiff(before, after);
        std::printf("  seed %llu: mean |diff| rerender=%.4f, after save/load=%.4f\n",
                    static_cast<unsigned long long>(seed), noise, drift);
        std::snprintf(desc, sizeof(desc), "seed %llu: renders the same after save/load",
                      static_cast<unsigned long long>(seed));
        check(drift <= noise * 1.5 + 0.05, desc);
    }
}

} // namespace

int main() {
    testParameterNamesAreUnambiguous();
    testEveryVariationRoundTrips();
    testEditedRandomFlames();
    return apo_test::reportAndExit();
}
