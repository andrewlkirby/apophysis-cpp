// Tests for Phase 4's GradientIO (src/core/io/GradientIO.h): .map and .ugr
// gradient file loading, verified directly against ColorMap/cmap.pas /
// Forms/Adjust.pas's SaveMap.

#include <cstdio>
#include <fstream>
#include <string>

#include "TestHelpers.h"
#include "core/ColorMap.h"
#include "core/io/GradientIO.h"

using apo_test::check;

namespace {

void writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    out << content;
}

void testLoadMapFile() {
    std::string content;
    for (int i = 0; i < 256; ++i) {
        content += "  " + std::to_string(i) + "  " + std::to_string(255 - i) + "  0\n";
    }
    const std::string path = "gradient_io_test.map";
    writeFile(path, content);

    apo::ColorMap cmap{};
    check(apo::loadGradientFile(path, cmap), "loadGradientFile succeeds for a .map file");
    check(cmap.entries[0][0] == 0 && cmap.entries[0][1] == 255, "map: first entry parses");
    check(cmap.entries[255][0] == 255 && cmap.entries[255][1] == 0, "map: last entry parses");
    check(cmap.entries[128][0] == 128 && cmap.entries[128][1] == 127, "map: a middle entry parses");

    std::remove(path.c_str());
}

void testLoadMapFileToleratesTrailingComment() {
    // SaveMap appends a comment after the first line's RGB triple.
    const std::string path = "gradient_io_test_comment.map";
    writeFile(path, "  10  20  30  Exported from Apophysis 2.0\n  40  50  60\n");

    apo::ColorMap cmap{};
    check(apo::loadGradientFile(path, cmap), "loadGradientFile tolerates a trailing comment on line 1");
    check(cmap.entries[0][0] == 10 && cmap.entries[0][1] == 20 && cmap.entries[0][2] == 30,
          "map: first entry parses correctly despite the trailing comment");
    // Only 2 real lines were given - the rest should repeat the last one
    // rather than being left black.
    check(cmap.entries[255][0] == 40 && cmap.entries[255][1] == 50 && cmap.entries[255][2] == 60,
          "map: a file shorter than 256 lines pads with the last entry");

    std::remove(path.c_str());
}

void testLoadUgrNamedBlock() {
    const std::string content =
        "first_gradient {\n"
        "gradient:\n"
        "  index=0 color=255\n"
        "  index=399 color=16711680\n"
        "}\n"
        "second_gradient {\n"
        "gradient:\n"
        "  index=0 color=65280\n"
        "  index=399 color=65280\n"
        "}\n";
    const std::string path = "gradient_io_test.ugr";
    writeFile(path, content);

    // Color decoding matches GetPalette exactly: R = color mod 256,
    // G = (color div 256) mod 256, B = color div 65536 - so color=65280
    // (0x00FF00) is solid green, color=255 is R=255 (red), and
    // color=16711680 (0xFF0000 as a plain decimal int) is B=255 (blue).
    apo::ColorMap cmap{};
    check(apo::loadGradientFile(path, cmap, "second_gradient"), "loadGradientFile finds a named .ugr block");
    check(cmap.entries[0][1] == 255 && cmap.entries[0][0] == 0,
          "ugr: the selected block's (not the first block's) color is used - solid green, not the first block's red");

    apo::ColorMap firstCmap{};
    check(apo::loadGradientFile(path, firstCmap), "loadGradientFile defaults to the first block when unnamed");
    check(firstCmap.entries[0][0] == 255, "ugr: the default (first) block starts red (index=0 color=255)");
    check(firstCmap.entries[255][2] == 255, "ugr: the default (first) block ends blue (index=399 color=16711680)");

    check(!apo::loadGradientFile(path, cmap, "no_such_gradient"), "loadGradientFile fails for an unknown block name");

    const auto names = apo::listUgrGradientNames(path);
    check(names.size() == 2 && names[0] == "first_gradient" && names[1] == "second_gradient",
          "listUgrGradientNames enumerates both blocks in file order");

    std::remove(path.c_str());
}

void testMissingFileFails() {
    apo::ColorMap cmap{};
    check(!apo::loadGradientFile("does_not_exist_12345.map", cmap), "loading a nonexistent .map file fails cleanly");
    check(!apo::loadGradientFile("does_not_exist_12345.ugr", cmap), "loading a nonexistent .ugr file fails cleanly");
}

} // namespace

int main() {
    testLoadMapFile();
    testLoadMapFileToleratesTrailingComment();
    testLoadUgrNamedBlock();
    testMissingFileFails();
    return apo_test::reportAndExit();
}
