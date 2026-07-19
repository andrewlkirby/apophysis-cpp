// Tests for Phase 4's FlameIO (src/core/io/FlameIO.h) - .flame/.flam3 XML
// load/save via pugixml, verified directly against Flame/ParameterIO.pas.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include "TestHelpers.h"
#include "core/Flame.h"
#include "core/VariationRegistry.h"
#include "core/io/FlameIO.h"

using apo_test::approxEqual;
using apo_test::check;

namespace {

std::string tempPath(const std::string& name) {
    // Not using the real scratchpad dir from the harness (this is a C++
    // test binary run standalone, not this Claude session) - a fixed,
    // predictable name in the working directory ctest already runs each
    // test executable from is simplest and doesn't need <filesystem>.
    return name;
}

std::unique_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_unique<apo::Flame>();
    flame->name = "roundtrip test";
    flame->width = 320;
    flame->height = 240;
    flame->center = {1.5, -2.5};
    flame->pixelsPerUnit = 80;
    flame->sampleDensity = 100;
    flame->zoom = 0.5;
    flame->angle = 0.25; // exercises the fixed rotate/angle round-trip
    flame->vibrancy = 0.8;
    flame->brightness = 3.5;
    flame->gamma = 3.2;
    flame->gammaThreshold = 0.02;
    flame->spatialOversample = 2;
    flame->spatialFilterRadius = 0.6;
    flame->background = {12, 200, 77, 0}; // exercises the fixed background round-trip
    flame->cameraPitch = 0.1;
    flame->cameraYaw = 0.2;
    flame->cameraPersp = 0.9;
    flame->cameraZpos = 0.3;
    flame->cameraDOF = 0.05;
    flame->estimator = 5;
    flame->estimatorMin = 1;
    flame->estimatorCurve = 0.3;
    flame->enableDE = true;
    flame->nbatches = 3;
    flame->time = 7.5;
    flame->soloXform = 1;

    // Non-default Curves-dialog tone curves - a "brighten the master
    // channel, darken red" edit, distinctive enough to catch a
    // channel-order or point/weight-order mixup on round-trip.
    flame->curves[0].points[1] = {0.1, 0.3};
    flame->curves[0].points[2] = {0.7, 0.9};
    flame->curves[0].weights[1] = 1.5;
    flame->curves[1].points[1] = {0.2, 0.0};
    flame->curves[1].weights[2] = 2.0;

    for (int i = 0; i < 256; ++i) {
        flame->cmap.entries[i][0] = i;
        flame->cmap.entries[i][1] = 255 - i;
        flame->cmap.entries[i][2] = (i * 2) % 256;
        flame->cmap.entries[i][3] = 255;
    }

    apo::XForm& x0 = *flame->xform[0];
    x0.clear();
    x0.density = 0.4;
    x0.color = 0.1;
    x0.symmetry = 0.3;
    x0.transOpacity = 0.9;
    x0.transformName = "first";
    x0.c[0] = {0.6, 0.1};
    x0.c[1] = {-0.1, 0.6};
    x0.c[2] = {0.2, -0.3};
    x0.setVariation(0, 0.0);
    x0.setVariation(3, 1.0); // "spherical" (local index 3)

    apo::XForm& x1 = *flame->xform[1];
    x1.clear();
    x1.density = 0.6;
    x1.color = 0.9;
    x1.c[0] = {0.5, 0};
    x1.c[1] = {0, 0.5};
    x1.c[2] = {0.1, 0.1};
    x1.setVariation(0, 1.0); // "linear" only

    flame->finalXformEnabled = true;
    flame->useFinalXform = true;
    apo::XForm& fx = *flame->finalXform;
    fx.clear();
    fx.c[2] = {0.05, -0.05};
    fx.setVariation(0, 1.0);

    return flame;
}

void testRoundTrip() {
    auto original = makeTestFlame();
    const std::string path = tempPath("roundtrip_test.flame");

    check(apo::saveFlameFile(path, {original.get()}), "saveFlameFile succeeds");

    auto loaded = apo::loadFlameFile(path);
    check(loaded.size() == 1, "loadFlameFile returns exactly one flame");
    if (loaded.empty()) {
        std::remove(path.c_str());
        return;
    }
    const apo::Flame& f = *loaded[0];

    check(f.name == "roundtrip test", "name round-trips");
    check(f.width == 320 && f.height == 240, "size round-trips");
    check(approxEqual(f.center[0], 1.5) && approxEqual(f.center[1], -2.5), "center round-trips");
    check(approxEqual(f.pixelsPerUnit, 80), "scale (pixelsPerUnit) round-trips");
    check(approxEqual(f.sampleDensity, 100), "quality (sampleDensity) round-trips");
    check(approxEqual(f.zoom, 0.5), "zoom round-trips");
    check(approxEqual(f.angle, 0.25, 1e-6), "angle round-trips (the fixed rotate/angle bug)");
    check(approxEqual(f.vibrancy, 0.8), "vibrancy round-trips");
    check(approxEqual(f.brightness, 3.5), "brightness round-trips");
    check(approxEqual(f.gamma, 3.2), "gamma round-trips");
    check(approxEqual(f.gammaThreshold, 0.02), "gamma_threshold round-trips");
    check(f.spatialOversample == 2, "oversample round-trips");
    check(approxEqual(f.spatialFilterRadius, 0.6), "filter (spatialFilterRadius) round-trips");
    check(f.background[0] == 12 && f.background[1] == 200 && f.background[2] == 77,
          "background round-trips exactly (the fixed integer-encoding bug)");
    check(approxEqual(f.cameraPitch, 0.1) && approxEqual(f.cameraYaw, 0.2) && approxEqual(f.cameraPersp, 0.9) &&
              approxEqual(f.cameraZpos, 0.3) && approxEqual(f.cameraDOF, 0.05),
          "camera fields round-trip");
    check(approxEqual(f.estimator, 5) && approxEqual(f.estimatorMin, 1) && approxEqual(f.estimatorCurve, 0.3),
          "estimator fields round-trip");
    check(f.enableDE, "enable_de round-trips");
    check(f.nbatches == 3, "batches round-trips");
    check(approxEqual(f.time, 7.5), "time round-trips");
    check(f.soloXform == 1, "soloxform round-trips");

    check(approxEqual(f.curves[0].points[1][0], 0.1) && approxEqual(f.curves[0].points[1][1], 0.3) &&
              approxEqual(f.curves[0].points[2][0], 0.7) && approxEqual(f.curves[0].points[2][1], 0.9) &&
              approxEqual(f.curves[0].weights[1], 1.5),
          "curves[0] (master channel) points/weight round-trip");
    check(approxEqual(f.curves[1].points[1][0], 0.2) && approxEqual(f.curves[1].points[1][1], 0.0) &&
              approxEqual(f.curves[1].weights[2], 2.0),
          "curves[1] (red channel) points/weight round-trip");
    check(f.curves[2] == apo::BezierCurve{} && f.curves[3] == apo::BezierCurve{},
          "untouched curves[2]/curves[3] (green/blue) stay at the default identity curve");

    check(f.numXForms() == 2, "xform count round-trips");
    if (f.numXForms() == 2) {
        const apo::XForm& lx0 = *f.xform[0];
        check(approxEqual(lx0.density, 0.4), "xform[0].weight round-trips");
        check(approxEqual(lx0.color, 0.1), "xform[0].color round-trips");
        check(approxEqual(lx0.symmetry, 0.3), "xform[0].symmetry(color_speed) round-trips");
        check(approxEqual(lx0.transOpacity, 0.9), "xform[0].opacity round-trips");
        check(lx0.transformName == "first", "xform[0].name round-trips");
        check(approxEqual(lx0.c[0][0], 0.6) && approxEqual(lx0.c[2][1], -0.3), "xform[0].coefs round-trip");
        check(approxEqual(lx0.variation(0), 0.0), "xform[0] linear weight is 0, not the XForm::clear() default of 1 "
                                                   "(variation weights are zeroed before applying file attributes)");
        check(approxEqual(lx0.variation(3), 1.0), "xform[0] spherical weight round-trips");

        const apo::XForm& lx1 = *f.xform[1];
        check(approxEqual(lx1.variation(0), 1.0), "xform[1] linear weight round-trips");
    }

    check(f.finalXformEnabled && f.useFinalXform, "finalxform presence round-trips");
    check(approxEqual(f.finalXform->c[2][0], 0.05) && approxEqual(f.finalXform->c[2][1], -0.05),
          "finalxform coefs round-trip");
    check(approxEqual(f.finalXform->symmetry, 1.0) && approxEqual(f.finalXform->color, 0.0),
          "finalxform symmetry/color are forced to 1/0, matching the original");

    check(f.cmap.entries[0][0] == 0 && f.cmap.entries[0][1] == 255 && f.cmap.entries[255][0] == 255,
          "palette round-trips");

    std::remove(path.c_str());
}

void testLoadHandAuthoredFile() {
    // A minimal but realistic 2-xform flame, hand-written to the schema
    // confirmed directly against ParameterIO.pas - exercises loading a
    // file this port didn't itself produce, not just its own round-trip.
    const std::string xml =
        "<flame name=\"hand\" size=\"100 80\" center=\"0 0\" scale=\"40\" quality=\"10\" "
        "oversample=\"1\" filter=\"0.3\" background=\"10 20 30\" brightness=\"4\" gamma=\"4\">\n"
        "  <xform weight=\"0.5\" color=\"0\" linear=\"1\" coefs=\"0.5 0 0 0.5 0 0\" opacity=\"1\"/>\n"
        "  <xform weight=\"0.5\" color=\"1\" spherical=\"1\" coefs=\"0.5 0 0 -0.5 0.25 0.5\" opacity=\"1\"/>\n"
        "  <palette count=\"256\" format=\"RGB\">\n"
        "      000000010101020202030303040404050505060606070707\n"
        "  </palette>\n"
        "</flame>\n";

    const std::string path = tempPath("handauthored_test.flame");
    {
        std::ofstream out(path);
        out << xml;
    }

    auto loaded = apo::loadFlameFile(path);
    check(loaded.size() == 1, "hand-authored file loads as exactly one flame");
    if (!loaded.empty()) {
        const apo::Flame& f = *loaded[0];
        check(f.name == "hand", "hand-authored: name parses");
        check(f.width == 100 && f.height == 80, "hand-authored: size parses");
        check(approxEqual(f.pixelsPerUnit, 40), "hand-authored: scale parses");
        check(f.background[0] == 10 && f.background[1] == 20 && f.background[2] == 30,
              "hand-authored: background parses");
        check(f.numXForms() == 2, "hand-authored: xform count parses");
        if (f.numXForms() == 2) {
            check(approxEqual(f.xform[0]->variation(0), 1.0), "hand-authored: xform[0] linear=1 parses");
            check(approxEqual(f.xform[1]->variation(3), 1.0), "hand-authored: xform[1] spherical=1 parses");
        }
        check(f.cmap.entries[0][0] == 0 && f.cmap.entries[1][0] == 1 && f.cmap.entries[7][0] == 7,
              "hand-authored: palette hex parses in order");
    }

    std::remove(path.c_str());
}

void testMissingFileReturnsEmpty() {
    auto loaded = apo::loadFlameFile("this_file_does_not_exist_12345.flame");
    check(loaded.empty(), "loading a nonexistent file returns an empty vector, not a crash");
}

void testDefaultCurvesAreNotWritten() {
    // A flame that never touched the Curves dialog should produce a
    // `curves`-attribute-free file (keeps the common case's saved XML
    // clean, matches FlameIO.cpp's writeFlame() only-if-non-default gate).
    auto flame = std::make_unique<apo::Flame>();
    flame->name = "no curves";
    apo::XForm& xf = *flame->xform[0];
    xf.clear();
    xf.density = 1.0;

    const std::string path = tempPath("no_curves_test.flame");
    check(apo::saveFlameFile(path, {flame.get()}), "saveFlameFile succeeds for a default-curves flame");

    std::ifstream in(path);
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    check(contents.find("curves=") == std::string::npos,
          "a flame with only default (untouched) curves omits the `curves` attribute entirely");

    std::remove(path.c_str());
}

void testStringRoundTrip() {
    // Same fixture/coverage shape as testRoundTrip(), but through
    // saveFlameToString/loadFlameFromString instead of the file path - the
    // pair MainWindow's clipboard Copy/Paste (A2) is built on.
    auto original = makeTestFlame();
    const std::string xml = apo::saveFlameToString({original.get()});
    check(!xml.empty(), "saveFlameToString produces non-empty XML");
    check(xml.find("<flame") != std::string::npos, "saveFlameToString's output looks like a <flame> element");

    auto loaded = apo::loadFlameFromString(xml);
    check(loaded.size() == 1, "loadFlameFromString returns exactly one flame");
    if (loaded.empty()) return;
    const apo::Flame& f = *loaded[0];

    check(f.name == "roundtrip test", "name round-trips through the string path");
    check(f.numXForms() == 2, "xform count round-trips through the string path");
    check(approxEqual(f.xform[0]->c[0][0], 0.6) && approxEqual(f.xform[0]->c[2][1], -0.3),
          "xform coefs round-trip through the string path");
    check(f.finalXformEnabled, "finalxform presence round-trips through the string path");
}

void testLoadFromStringRejectsGarbage() {
    auto loaded = apo::loadFlameFromString("not xml at all { }");
    check(loaded.empty(), "loadFlameFromString returns an empty vector for unparseable text, not a crash");
}

void testSaveToStringHandlesMultipleFlames() {
    auto a = makeTestFlame();
    a->name = "first";
    auto b = makeTestFlame();
    b->name = "second";
    const std::string xml = apo::saveFlameToString({a.get(), b.get()});

    auto loaded = apo::loadFlameFromString(xml);
    check(loaded.size() == 2, "saveFlameToString/loadFlameFromString round-trip multiple flames via a <flames> wrapper");
    if (loaded.size() == 2) {
        check(loaded[0]->name == "first" && loaded[1]->name == "second",
              "multi-flame string round-trip preserves order and names");
    }
}

} // namespace

int main() {
    testRoundTrip();
    testLoadHandAuthoredFile();
    testMissingFileReturnsEmpty();
    testDefaultCurvesAreNotWritten();
    testStringRoundTrip();
    testLoadFromStringRejectsGarbage();
    testSaveToStringHandlesMultipleFlames();
    return apo_test::reportAndExit();
}
