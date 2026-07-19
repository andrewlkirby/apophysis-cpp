#include "GradientOps.h"

#include <algorithm>
#include <cmath>

namespace apo {

namespace {

int clampByte(int v) { return std::clamp(v, 0, 255); }
int mod256(int v) { return ((v % 256) + 256) % 256; }

} // namespace

ColorMap rotateGradient(const ColorMap& in, int offset) {
    ColorMap out{};
    for (int i = 0; i < 256; ++i) {
        out.entries[i] = in.entries[mod256(i - offset)];
    }
    return out;
}

ColorMap hueShiftGradient(const ColorMap& in, double degrees) {
    ColorMap out{};
    const double shift = degrees / 60.0; // rgbToHsv/hsvToRgb use sextant units (hue in [0,6))
    for (int i = 0; i < 256; ++i) {
        double rgb[3] = {in.entries[i][0] / 255.0, in.entries[i][1] / 255.0, in.entries[i][2] / 255.0};
        double hsv[3];
        rgbToHsv(rgb, hsv);
        hsv[0] = std::fmod(hsv[0] + shift, 6.0);
        if (hsv[0] < 0) hsv[0] += 6.0;
        hsvToRgb(hsv, rgb);
        out.entries[i][0] = clampByte(static_cast<int>(std::lround(rgb[0] * 255)));
        out.entries[i][1] = clampByte(static_cast<int>(std::lround(rgb[1] * 255)));
        out.entries[i][2] = clampByte(static_cast<int>(std::lround(rgb[2] * 255)));
        out.entries[i][3] = in.entries[i][3];
    }
    return out;
}

ColorMap saturationShiftGradient(const ColorMap& in, double delta) {
    ColorMap out{};
    for (int i = 0; i < 256; ++i) {
        double rgb[3] = {in.entries[i][0] / 255.0, in.entries[i][1] / 255.0, in.entries[i][2] / 255.0};
        double hsv[3];
        rgbToHsv(rgb, hsv);
        if (hsv[1] == 0) {
            out.entries[i] = in.entries[i];
            continue;
        }
        hsv[1] = std::clamp(hsv[1] + delta / 100.0, 0.0, 1.0);
        hsvToRgb(hsv, rgb);
        out.entries[i][0] = clampByte(static_cast<int>(std::lround(rgb[0] * 255)));
        out.entries[i][1] = clampByte(static_cast<int>(std::lround(rgb[1] * 255)));
        out.entries[i][2] = clampByte(static_cast<int>(std::lround(rgb[2] * 255)));
        out.entries[i][3] = in.entries[i][3];
    }
    return out;
}

ColorMap brightnessShiftGradient(const ColorMap& in, int delta) {
    ColorMap out{};
    for (int i = 0; i < 256; ++i) {
        out.entries[i][0] = clampByte(in.entries[i][0] + delta);
        out.entries[i][1] = clampByte(in.entries[i][1] + delta);
        out.entries[i][2] = clampByte(in.entries[i][2] + delta);
        out.entries[i][3] = in.entries[i][3];
    }
    return out;
}

ColorMap contrastShiftGradient(const ColorMap& in, int amount) {
    const int intens = amount > 0 ? amount * 2 : amount;
    ColorMap out{};
    for (int i = 0; i < 256; ++i) {
        for (int ch = 0; ch < 3; ++ch) {
            const int v = in.entries[i][ch];
            out.entries[i][ch] = clampByte(static_cast<int>(std::lround(v + intens / 100.0 * (v - 127))));
        }
        out.entries[i][3] = in.entries[i][3];
    }
    return out;
}

ColorMap blurGradient(const ColorMap& in, int radius) {
    if (radius <= 0) return in;
    ColorMap out{};
    for (int i = 0; i < 256; ++i) {
        int sum[3] = {0, 0, 0};
        for (int j = i - radius; j <= i + radius; ++j) {
            if (j == i) continue;
            const int k = mod256(j);
            sum[0] += in.entries[k][0];
            sum[1] += in.entries[k][1];
            sum[2] += in.entries[k][2];
        }
        const int n = 2 * radius;
        out.entries[i][0] = sum[0] / n;
        out.entries[i][1] = sum[1] / n;
        out.entries[i][2] = sum[2] / n;
        out.entries[i][3] = in.entries[i][3];
    }
    return out;
}

ColorMap frequencyGradient(const ColorMap& in, int times) {
    if (times <= 1) return in;
    ColorMap out = in;
    const int n = 256 / times;
    for (int j = 0; j <= times; ++j) {
        for (int i = 0; i <= n; ++i) {
            const int destIndex = i + j * n;
            if (destIndex >= 256) continue;
            // i*times can reach exactly 256 when i==n (e.g. times=2 ->
            // n=128 -> i*times=256) - the original Delphi (Adjust.pas's
            // Frequency) reads pal[i*times] there too, one past a 0..255
            // array, which is undefined/out-of-bounds rather than the
            // intended periodic wraparound. Fixed here via mod 256 instead
            // of reproducing the out-of-bounds read.
            out.entries[destIndex] = in.entries[(i * times) % 256];
        }
    }
    return out;
}

ColorMap invertGradient(const ColorMap& in) {
    ColorMap out{};
    for (int i = 0; i < 256; ++i) {
        out.entries[i][0] = 255 - in.entries[i][0];
        out.entries[i][1] = 255 - in.entries[i][1];
        out.entries[i][2] = 255 - in.entries[i][2];
        out.entries[i][3] = in.entries[i][3];
    }
    return out;
}

ColorMap reverseGradient(const ColorMap& in) {
    ColorMap out{};
    for (int i = 0; i < 256; ++i) {
        out.entries[i] = in.entries[255 - i];
    }
    return out;
}

ColorMap randomGradient(Rng& rng) {
    constexpr int kMinNodes = 3;
    constexpr int kMaxNodes = 8;
    const int nodes = kMinNodes + rng.uniformInt(kMaxNodes - kMinNodes + 1);
    const int n = 256 / nodes;

    auto placeRandomNode = [&rng](ColorMap& pal, int index) {
        const double hue = rng.uniform01() * 6.0; // full range, sextant units
        const double sat = 0.4 + rng.uniform01() * 0.6;  // 40-100% saturation
        const double val = 0.35 + rng.uniform01() * 0.6; // 35-95% luminance
        const double hsv[3] = {hue, sat, val};
        double rgb[3];
        hsvToRgb(hsv, rgb);
        pal.entries[index][0] = clampByte(static_cast<int>(std::lround(rgb[0] * 255)));
        pal.entries[index][1] = clampByte(static_cast<int>(std::lround(rgb[1] * 255)));
        pal.entries[index][2] = clampByte(static_cast<int>(std::lround(rgb[2] * 255)));
    };

    ColorMap pal{};
    placeRandomNode(pal, 0);
    int b = 0;
    while (b < 255) {
        const int a = b;
        b = std::min(b + n, 255);
        placeRandomNode(pal, b);
        rgbBlendBetween(a, b, pal);
    }
    return pal;
}

} // namespace apo
