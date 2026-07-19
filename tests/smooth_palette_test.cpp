// Tests for A5's Smooth Palette (src/core/edit/SmoothPalette.h) - Draves'
// image-to-gradient algorithm ported from Main.pas:4165. Kept Qt-free and
// unit-tested independently of the dialog, mirroring gradient_ops_test.cpp's
// approach.

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

#include "TestHelpers.h"
#include "core/Rng.h"
#include "core/edit/SmoothPalette.h"

using apo_test::check;

namespace {

// A deterministic, colorful (not just 1-2 flat colors) synthetic test
// image - varied enough that "smoother" vs "less smooth" orderings of a
// sample of its pixels are actually meaningfully different.
std::vector<std::uint8_t> makeColorfulImage(int width, int height) {
    std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3;
            pixels[idx + 0] = static_cast<std::uint8_t>((x * 37 + y * 13) % 256);
            pixels[idx + 1] = static_cast<std::uint8_t>((x * 91 + y * 59) % 256);
            pixels[idx + 2] = static_cast<std::uint8_t>((x * 17 + y * 131) % 256);
        }
    }
    return pixels;
}

int64_t totalAdjacentDistance(const apo::ColorMap& cmap) {
    int64_t total = 0;
    for (int i = 0; i < 255; ++i) {
        const int dr = cmap.entries[static_cast<size_t>(i + 1)][0] - cmap.entries[static_cast<size_t>(i)][0];
        const int dg = cmap.entries[static_cast<size_t>(i + 1)][1] - cmap.entries[static_cast<size_t>(i)][1];
        const int db = cmap.entries[static_cast<size_t>(i + 1)][2] - cmap.entries[static_cast<size_t>(i)][2];
        total += static_cast<int64_t>(dr) * dr + static_cast<int64_t>(dg) * dg + static_cast<int64_t>(db) * db;
    }
    return total;
}

void testInvalidInputReturnsFalse() {
    apo::ColorMap out{};
    apo::Xoshiro256Rng rng(1);
    apo::SmoothPaletteParams params;
    check(!apo::smoothPaletteFromImage(nullptr, 10, 10, rng, params, out), "a null pixel buffer returns false");

    std::vector<std::uint8_t> pixels(300, 0);
    check(!apo::smoothPaletteFromImage(pixels.data(), 0, 10, rng, params, out), "a zero width returns false");
    check(!apo::smoothPaletteFromImage(pixels.data(), 10, 0, rng, params, out), "a zero height returns false");
    check(!apo::smoothPaletteFromImage(pixels.data(), -5, 10, rng, params, out), "a negative width returns false");
}

void testOutputEntriesAreExactSourcePixels() {
    const int w = 16, h = 16;
    const auto pixels = makeColorfulImage(w, h);

    apo::Xoshiro256Rng rng(42);
    apo::SmoothPaletteParams params;
    params.numTries = 2;
    params.tryLength = 500; // small - this test cares about correctness, not smoothing quality
    apo::ColorMap out{};
    check(apo::smoothPaletteFromImage(pixels.data(), w, h, rng, params, out),
          "smoothPaletteFromImage succeeds on a valid image");

    bool everyEntryIsARealSourcePixel = true;
    for (int i = 0; i < 256 && everyEntryIsARealSourcePixel; ++i) {
        const auto& e = out.entries[static_cast<size_t>(i)];
        bool foundMatch = false;
        for (int y = 0; y < h && !foundMatch; ++y) {
            for (int x = 0; x < w && !foundMatch; ++x) {
                const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3;
                if (pixels[idx] == e[0] && pixels[idx + 1] == e[1] && pixels[idx + 2] == e[2]) foundMatch = true;
            }
        }
        if (!foundMatch) everyEntryIsARealSourcePixel = false;
    }
    check(everyEntryIsARealSourcePixel,
          "every output entry is an exact color sampled from the source image (a reorder, never a blend)");
    check(out.entries[0][3] == 255, "output alpha is always fully opaque");
}

void testResultIsDeterministicForAFixedSeed() {
    const int w = 20, h = 20;
    const auto pixels = makeColorfulImage(w, h);
    apo::SmoothPaletteParams params;
    params.numTries = 3;
    params.tryLength = 1000;

    apo::Xoshiro256Rng rngA(777);
    apo::ColorMap outA{};
    check(apo::smoothPaletteFromImage(pixels.data(), w, h, rngA, params, outA), "first run succeeds");

    apo::Xoshiro256Rng rngB(777);
    apo::ColorMap outB{};
    check(apo::smoothPaletteFromImage(pixels.data(), w, h, rngB, params, outB), "second run succeeds");

    check(outA.entries == outB.entries, "the same seed/image/params reproduces byte-identical output");
}

void testDifferentSeedsProduceDifferentResults() {
    const int w = 20, h = 20;
    const auto pixels = makeColorfulImage(w, h);
    apo::SmoothPaletteParams params;
    params.numTries = 3;
    params.tryLength = 1000;

    apo::Xoshiro256Rng rngA(1);
    apo::ColorMap outA{};
    apo::smoothPaletteFromImage(pixels.data(), w, h, rngA, params, outA);

    apo::Xoshiro256Rng rngB(2);
    apo::ColorMap outB{};
    apo::smoothPaletteFromImage(pixels.data(), w, h, rngB, params, outB);

    check(!(outA.entries == outB.entries), "different seeds produce different sampled/reordered palettes");
}

void testAlgorithmReducesAdjacentColorDistanceVersusARandomOrdering() {
    const int w = 24, h = 24;
    const auto pixels = makeColorfulImage(w, h);
    apo::SmoothPaletteParams params; // defaults: numTries=10, tryLength=100000

    apo::Xoshiro256Rng rng(2024);
    apo::ColorMap smoothed{};
    check(apo::smoothPaletteFromImage(pixels.data(), w, h, rng, params, smoothed), "smoothPaletteFromImage succeeds");

    apo::ColorMap shuffled = smoothed;
    std::mt19937 shuffleRng(123);
    std::shuffle(shuffled.entries.begin(), shuffled.entries.end(), shuffleRng);

    check(totalAdjacentDistance(smoothed) < totalAdjacentDistance(shuffled),
          "the smoothed ordering has a lower total adjacent-color distance than a random shuffle of the exact same "
          "256 sampled colors - the whole point of the algorithm");
}

} // namespace

int main() {
    testInvalidInputReturnsFalse();
    testOutputEntriesAreExactSourcePixels();
    testResultIsDeterministicForAFixedSeed();
    testDifferentSeedsProduceDifferentResults();
    testAlgorithmReducesAdjacentColorDistanceVersusARandomOrdering();
    return apo_test::reportAndExit();
}
