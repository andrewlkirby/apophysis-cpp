// Broad smoke test across every statically-linked Plugin/*.c wrapper (see
// src/core/plugins/) - not a substitute for plugin_variations_test.cpp's
// hand-verified checks on the two prototype plugins, but this is what
// actually exercises the other 45: registration, a calc() call through the
// full XForm pipeline for a handful of representative points, and that the
// output is always finite (no NaN/Inf, no crash) - the kind of mistake a
// mechanical per-file port is most likely to make (wrong prefix, dangling
// closure-captured `vp`, wrong registered name).

#include <cmath>
#include <string>
#include <vector>

#include "TestHelpers.h"
#include "core/Rng.h"
#include "core/VariationRegistry.h"
#include "core/XForm.h"

using apo_test::check;

namespace {

// Every plugin registered in src/core/plugins/generated/, by its actual
// registered name (see PluginVariation.h's registerPluginVariation calls) -
// note stwins/swirl2 register under "stwin"/"psphere", not their filename.
const char* kPluginNames[] = {
    "bcircle", "barycentroid", "boarders2", "coswrap", "dc_boarders", "dc_bubble", "dc_carpet", "dc_cube",
    "dc_gridout", "dc_linear", "dc_mandelbrot", "dc_triangle", "dc_ztransl", "ex", "extrude", "falloff",
    "fisheye", "gdoffs", "handkerchief", "hexcrop", "julia", "julian2", "julian2dc", "npolar", "octapol",
    "polynomial", "post_bwraps2", "post_dcztransl", "post_julian2", "post_mirror_x", "post_mirror_y",
    "post_mirror_z", "post_polar2", "post_rblur", "pre_boarders2", "pre_bwraps2", "pre_dcztransl", "pre_polar2",
    "ripple", "sigmoid", "sinusgrid", "spherical2", "stwin", "psphere", "wavesn", "xheart", "xtrb",
};

int registeredIndex(const std::string& name) {
    return apo::VariationRegistry::instance().variationIndex(name);
}

void testTotalRegisteredCount() {
    // 47 native Variations/*.pas ports (see src/core/variations/) + 47
    // Plugin/*.c wrappers (56 total .c files in the original, minus
    // dc_image.c - a bespoke image-loading plugin out of scope for this
    // port - minus example-plugin.c - a non-functional template - minus 7
    // whose registered name collides with an already-native-ported
    // variation: auger, crop, falloff2, post_crop, pre_crop,
    // post_falloff2, pre_falloff2. A silent drop here (e.g. a wrapper file
    // that failed to self-register) would otherwise go unnoticed.
    const int kExpectedNative = 47;
    const int kExpectedPlugins = 47;
    check(apo::VariationRegistry::instance().numRegisteredVariations() == kExpectedNative + kExpectedPlugins,
          "total registered (native + plugin) variation count matches expectations");
}

void testAllPluginsRegistered() {
    for (const char* name : kPluginNames) {
        check(registeredIndex(name) >= apo::VariationRegistry::kNumLocalVars,
              (std::string(name) + " plugin is registered").c_str());
    }
}

void testAllPluginsProduceFiniteOutput() {
    // A few varied sample points - some plugins branch on sign/magnitude
    // (e.g. crop-like clamping, r<=1 vs r>1 circle tests), so one point
    // isn't enough to exercise most code paths at all. Deliberately avoids
    // the exact origin (0,0,0): several plugins (npolar, octapol,
    // pre/post_polar2, dc_boarders' exact-grid-line case) take log(0) or
    // 0/0 there - a genuine singularity in the original math itself (not a
    // porting defect: any polar/log-based variation has one), which a
    // continuous chaos-game point stream has probability zero of ever
    // landing on exactly. Not something to "fix" by changing the plugins.
    const apo::Point3 samples[] = {
        {0.3, 0.4, 0.1}, {-0.7, 0.2, -0.3}, {0.001, -0.002, 0.0}, {1.5, -1.5, 0.5}, {-0.05, 0.9, -0.9},
    };

    for (const char* name : kPluginNames) {
        const int idx = registeredIndex(name);
        if (idx < apo::VariationRegistry::kNumLocalVars) continue; // already reported missing above

        bool allFinite = true;
        for (const apo::Point3& sample : samples) {
            apo::XForm x;
            // Unlike the hand-verified per-variation tests elsewhere (which
            // isolate the target variation by disabling linear), this
            // blends in linear at a modest weight: post_polar2 is a
            // "post"-style variation that only ever reads the accumulator
            // (FPx/FPy), never the input point - run alone, it'd see an
            // all-zero accumulator on every call and hit log(0) regardless
            // of the sample point, which isn't a porting defect, just how
            // post-variations compose. Blending in linear gives the
            // accumulator a real (nonzero) value first, matching how these
            // are actually used.
            x.setVariation(0, 0.5);
            x.setVariation(idx, 0.5);

            apo::MtRng rng(1);
            x.prepare(rng);

            apo::Point3 pt = sample;
            double colorCoord = 0.0;
            x.nextPoint(pt, colorCoord);

            if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) {
                allFinite = false;
                break;
            }
        }
        check(allFinite, (std::string(name) + " produces finite output across sample points").c_str());
    }
}

} // namespace

int main() {
    testTotalRegisteredCount();
    testAllPluginsRegistered();
    testAllPluginsProduceFiniteOutput();
    return apo_test::reportAndExit();
}
