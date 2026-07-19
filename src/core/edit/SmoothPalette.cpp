#include "SmoothPalette.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace apo {

namespace {
struct Rgb {
    int r = 0, g = 0, b = 0;
};

// Matches Main.pas's diffcolor exactly (a sum of squared per-channel
// differences - always non-negative, so the original's own `abs()` wrapper
// around it is a no-op, not reproduced here).
int64_t diffColor(const std::array<Rgb, 256>& list, int i, int j) {
    const int64_t dr = list[static_cast<size_t>(j)].r - list[static_cast<size_t>(i)].r;
    const int64_t dg = list[static_cast<size_t>(j)].g - list[static_cast<size_t>(i)].g;
    const int64_t db = list[static_cast<size_t>(j)].b - list[static_cast<size_t>(i)].b;
    return dr * dr + dg * dg + db * db;
}
} // namespace

bool smoothPaletteFromImage(const std::uint8_t* pixelsRgb, int width, int height, Rng& rng,
                             const SmoothPaletteParams& params, ColorMap& out) {
    if (pixelsRgb == nullptr || width <= 0 || height <= 0) return false;

    std::array<Rgb, 256> clist{};
    for (auto& c : clist) {
        const int x = rng.uniformInt(width);
        const int y = rng.uniformInt(height);
        const std::uint8_t* p = pixelsRgb + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3;
        c = {p[0], p[1], p[2]};
    }
    const std::array<Rgb, 256> original = clist;
    std::array<Rgb, 256> best = clist;
    int64_t bestLen = 0;

    const int numTries = std::max(1, params.numTries);
    const int tryLength = std::max(0, params.tryLength);

    for (int tryIt = 1; tryIt <= numTries; ++tryIt) {
        clist = original;

        // scramble
        for (int i = 0; i < 256; ++i) {
            const int rnd = rng.uniformInt(256);
            std::swap(clist[static_cast<size_t>(i)], clist[static_cast<size_t>(rnd)]);
        }

        // measure - see this function's own doc comment on the i in
        // [0,254] bound (the original reads one entry past the array here).
        int64_t len = 0;
        for (int i = 0; i < 255; ++i) len += diffColor(clist, i, i + 1);

        // improve
        for (int i = 0; i < tryLength; ++i) {
            const int i0 = 1 + rng.uniformInt(254);
            const int i1 = 1 + rng.uniformInt(254);
            int64_t asIs, swapd;
            if (i0 - i1 == 1) {
                asIs = diffColor(clist, i1 - 1, i1) + diffColor(clist, i0, i0 + 1);
                swapd = diffColor(clist, i1 - 1, i0) + diffColor(clist, i1, i0 + 1);
            } else if (i1 - i0 == 1) {
                asIs = diffColor(clist, i0 - 1, i0) + diffColor(clist, i1, i1 + 1);
                swapd = diffColor(clist, i0 - 1, i1) + diffColor(clist, i0, i1 + 1);
            } else {
                asIs = diffColor(clist, i0, i0 + 1) + diffColor(clist, i0, i0 - 1) + diffColor(clist, i1, i1 + 1) +
                       diffColor(clist, i1, i1 - 1);
                swapd = diffColor(clist, i1, i0 + 1) + diffColor(clist, i1, i0 - 1) + diffColor(clist, i0, i1 + 1) +
                        diffColor(clist, i0, i1 - 1);
            }
            if (swapd < asIs) {
                std::swap(clist[static_cast<size_t>(i0)], clist[static_cast<size_t>(i1)]);
                len = len + swapd - asIs;
            }
        }

        if (tryIt == 1 || len < bestLen) {
            best = clist;
            bestLen = len;
        }
    }

    clist = best;

    // clean - see this function's own doc comment on the i0 in [1,253]
    // bound (the original draws i0 from [1,254], one wider than is safe
    // here).
    for (int i = 0; i < 1024; ++i) {
        const int i0 = 1 + rng.uniformInt(253);
        const int i1 = i0 + 1;
        const int64_t asIs = diffColor(clist, i0 - 1, i0) + diffColor(clist, i1, i1 + 1);
        const int64_t swapd = diffColor(clist, i0 - 1, i1) + diffColor(clist, i0, i1 + 1);
        if (swapd < asIs) std::swap(clist[static_cast<size_t>(i0)], clist[static_cast<size_t>(i1)]);
    }

    for (int i = 0; i < 256; ++i) {
        const Rgb& c = clist[static_cast<size_t>(i)];
        out.entries[static_cast<size_t>(i)] = {c.r, c.g, c.b, 255};
    }
    return true;
}

} // namespace apo
