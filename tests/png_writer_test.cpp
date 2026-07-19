// Tests for Phase 4's PNG writer (src/core/io/PngWriter.h).

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

#include "TestHelpers.h"
#include "core/io/PngWriter.h"

using apo_test::check;

namespace {

void testWritesValidRgbPng() {
    const int w = 16, h = 8;
    std::vector<std::uint8_t> pixels(static_cast<size_t>(w) * h * 3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t off = (static_cast<size_t>(y) * w + x) * 3;
            pixels[off + 0] = static_cast<std::uint8_t>(x * 16);
            pixels[off + 1] = static_cast<std::uint8_t>(y * 16);
            pixels[off + 2] = 128;
        }
    }

    const char* path = "png_writer_rgb_test.png";
    check(apo::writePng(path, w, h, 3, pixels.data()), "writePng succeeds for an RGB buffer");

    std::ifstream in(path, std::ios::binary);
    check(in.good(), "the written PNG file can be reopened");
    unsigned char magic[8] = {};
    in.read(reinterpret_cast<char*>(magic), 8);
    const unsigned char kPngMagic[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    bool magicOk = true;
    for (int i = 0; i < 8; ++i) magicOk &= (magic[i] == kPngMagic[i]);
    check(magicOk, "the written file starts with the PNG magic header bytes");

    in.close();
    std::remove(path);
}

void testWritesValidRgbaPng() {
    const int w = 4, h = 4;
    std::vector<std::uint8_t> pixels(static_cast<size_t>(w) * h * 4, 0);
    for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 200; // alpha channel

    const char* path = "png_writer_rgba_test.png";
    check(apo::writePng(path, w, h, 4, pixels.data()), "writePng succeeds for an RGBA buffer");
    std::ifstream in(path, std::ios::binary);
    check(in.good(), "the written RGBA PNG file can be reopened");
    in.close();
    std::remove(path);
}

void testRejectsInvalidInputs() {
    std::vector<std::uint8_t> pixels(3, 0);
    check(!apo::writePng("should_not_exist.png", 0, 1, 3, pixels.data()), "rejects zero width");
    check(!apo::writePng("should_not_exist.png", 1, 1, 5, pixels.data()), "rejects an unsupported channel count");
    check(!apo::writePng("should_not_exist.png", 1, 1, 3, nullptr), "rejects a null pixel buffer");
}

} // namespace

int main() {
    testWritesValidRgbPng();
    testWritesValidRgbaPng();
    testRejectsInvalidInputs();
    return apo_test::reportAndExit();
}
