// SCRATCH diagnostic test - not a permanent part of the suite. Loads the
// user's actual reported flame file, round-trips it through save/load, and
// diffs every field/variation param to find any lossy encoding.

#include <cstdio>
#include <iostream>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/VariationRegistry.h"
#include "core/io/FlameIO.h"

using apo_test::approxEqual;

namespace {

void dumpXForm(const char* label, const apo::XForm& xf) {
    auto& registry = apo::VariationRegistry::instance();
    std::printf("  %s: density=%.10g color=%.10g pluginColor=%.10g symmetry=%.10g opacity=%.10g\n", label, xf.density,
                xf.color, xf.pluginColor, xf.symmetry, xf.transOpacity);
    std::printf("    coefs: %.10g %.10g %.10g %.10g %.10g %.10g\n", xf.c[0][0], xf.c[0][1], xf.c[1][0], xf.c[1][1],
                xf.c[2][0], xf.c[2][1]);
    for (int i = 0; i < xf.numVariations(); ++i) {
        double w = xf.variation(i);
        if (w == 0) continue;
        std::printf("    var[%d]=%s weight=%.10g\n", i, registry.varName(i).c_str(), w);
        if (i >= apo::VariationRegistry::kNumLocalVars) {
            const auto& factory = registry.registeredVariation(i - apo::VariationRegistry::kNumLocalVars);
            for (int j = 0; j < factory.numVariables(); ++j) {
                std::string name = factory.variableNameAt(j);
                double value = 0;
                xf.getVariable(name, value);
                std::printf("      %s = %.10g\n", name.c_str(), value);
            }
        }
    }
}

bool compareXForm(const char* label, const apo::XForm& a, const apo::XForm& b) {
    bool ok = true;
    auto& registry = apo::VariationRegistry::instance();
    if (!approxEqual(a.density, b.density, 1e-9)) {
        std::printf("MISMATCH %s density: %.10g vs %.10g\n", label, a.density, b.density);
        ok = false;
    }
    if (!approxEqual(a.color, b.color, 1e-9)) {
        std::printf("MISMATCH %s color: %.10g vs %.10g\n", label, a.color, b.color);
        ok = false;
    }
    if (!approxEqual(a.pluginColor, b.pluginColor, 1e-9)) {
        std::printf("MISMATCH %s pluginColor: %.10g vs %.10g\n", label, a.pluginColor, b.pluginColor);
        ok = false;
    }
    if (!approxEqual(a.symmetry, b.symmetry, 1e-9)) {
        std::printf("MISMATCH %s symmetry: %.10g vs %.10g\n", label, a.symmetry, b.symmetry);
        ok = false;
    }
    for (int k = 0; k < 3; ++k)
        for (int j = 0; j < 2; ++j) {
            if (!approxEqual(a.c[k][j], b.c[k][j], 1e-9)) {
                std::printf("MISMATCH %s coefs[%d][%d]: %.10g vs %.10g\n", label, k, j, a.c[k][j], b.c[k][j]);
                ok = false;
            }
        }
    for (int i = 0; i < a.numVariations(); ++i) {
        double wa = a.variation(i), wb = b.variation(i);
        if (!approxEqual(wa, wb, 1e-9)) {
            std::printf("MISMATCH %s var[%d]=%s weight: %.10g vs %.10g\n", label, i, registry.varName(i).c_str(), wa,
                        wb);
            ok = false;
        }
        if (wa == 0 && wb == 0) continue;
        if (i >= apo::VariationRegistry::kNumLocalVars) {
            const auto& factory = registry.registeredVariation(i - apo::VariationRegistry::kNumLocalVars);
            for (int j = 0; j < factory.numVariables(); ++j) {
                std::string name = factory.variableNameAt(j);
                double va = 0, vb = 0;
                a.getVariable(name, va);
                b.getVariable(name, vb);
                if (!approxEqual(va, vb, 1e-9)) {
                    std::printf("MISMATCH %s %s.%s: %.10g vs %.10g\n", label, registry.varName(i).c_str(),
                                name.c_str(), va, vb);
                    ok = false;
                }
            }
        }
    }
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "scratch_user_flame.flame";
    auto originalList = apo::loadFlameFile(path);
    if (originalList.empty()) {
        std::printf("FAILED TO LOAD %s\n", path);
        return 1;
    }
    const apo::Flame& original = *originalList[0];
    std::printf("Loaded flame '%s', %d xforms, finalXformEnabled=%d\n", original.name.c_str(), original.numXForms(),
                (int)original.finalXformEnabled);

    for (int i = 0; i < original.numXForms(); ++i) {
        char label[32];
        std::snprintf(label, sizeof(label), "xform[%d]", i);
        dumpXForm(label, *original.xform[i]);
    }

    const std::string tmpPath = "scratch_roundtrip_out.flame";
    if (!apo::saveFlameFile(tmpPath, {&original})) {
        std::printf("saveFlameFile FAILED\n");
        return 1;
    }
    auto reloadedList = apo::loadFlameFile(tmpPath);
    if (reloadedList.empty()) {
        std::printf("FAILED TO RELOAD %s\n", tmpPath.c_str());
        return 1;
    }
    const apo::Flame& reloaded = *reloadedList[0];

    bool allOk = true;
    allOk &= approxEqual(original.center[0], reloaded.center[0], 1e-9) &&
             approxEqual(original.center[1], reloaded.center[1], 1e-9);
    allOk &= approxEqual(original.pixelsPerUnit, reloaded.pixelsPerUnit, 1e-9);
    allOk &= approxEqual(original.sampleDensity, reloaded.sampleDensity, 1e-9);
    allOk &= approxEqual(original.zoom, reloaded.zoom, 1e-9);
    allOk &= approxEqual(original.vibrancy, reloaded.vibrancy, 1e-9);
    allOk &= approxEqual(original.gamma, reloaded.gamma, 1e-9);
    allOk &= approxEqual(original.gammaThreshold, reloaded.gammaThreshold, 1e-9);
    allOk &= approxEqual(original.brightness, reloaded.brightness, 1e-9);
    allOk &= original.spatialOversample == reloaded.spatialOversample;
    allOk &= approxEqual(original.spatialFilterRadius, reloaded.spatialFilterRadius, 1e-9);
    allOk &= approxEqual(original.estimator, reloaded.estimator, 1e-9);
    allOk &= approxEqual(original.estimatorCurve, reloaded.estimatorCurve, 1e-9);
    allOk &= original.enableDE == reloaded.enableDE;
    allOk &= original.numXForms() == reloaded.numXForms();

    if (original.numXForms() == reloaded.numXForms()) {
        for (int i = 0; i < original.numXForms(); ++i) {
            char label[32];
            std::snprintf(label, sizeof(label), "xform[%d]", i);
            allOk &= compareXForm(label, *original.xform[i], *reloaded.xform[i]);
        }
    }

    for (int i = 0; i < 256; ++i) {
        for (int c = 0; c < 3; ++c) {
            if (original.cmap.entries[i][c] != reloaded.cmap.entries[i][c]) {
                std::printf("MISMATCH palette[%d][%d]: %d vs %d\n", i, c, original.cmap.entries[i][c],
                            reloaded.cmap.entries[i][c]);
                allOk = false;
            }
        }
    }

    std::printf(allOk ? "\nALL FIELDS ROUND-TRIP IDENTICALLY\n" : "\nROUND-TRIP MISMATCHES FOUND (see above)\n");
    std::remove(tmpPath.c_str());
    return allOk ? 0 : 1;
}
