#include "ColorMap.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace apo {

namespace {

double maxOf3(double a, double b, double c) { return std::max(a, std::max(b, c)); }
double minOf3(double a, double b, double c) { return std::min(a, std::min(b, c)); }

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return s;
}

std::vector<std::string> tokenize(const std::string& text) {
    // The original (cmap.pas's GetTokens) splits on the space character
    // only, not other whitespace - a plain-text gradient format quirk. This
    // port splits on any whitespace instead, which only ever makes parsing
    // *more* permissive (real files always separate attributes with a
    // space; this additionally tolerates attributes split across lines).
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : text) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

std::string valueAfterEquals(const std::string& token) {
    const auto pos = token.find('=');
    if (pos == std::string::npos) return {};
    return token.substr(pos + 1);
}

} // namespace

void rgbToHsv(const double rgb[3], double hsv[3]) {
    const double maxval = maxOf3(rgb[0], rgb[1], rgb[2]);
    const double minval = minOf3(rgb[0], rgb[1], rgb[2]);

    hsv[2] = maxval;

    if (maxval > 0 && maxval != minval) {
        const double del = maxval - minval;
        hsv[1] = del / maxval;

        if (rgb[0] > rgb[1] && rgb[0] > rgb[2]) {
            hsv[0] = (rgb[1] - rgb[2]) / del;
        } else if (rgb[1] > rgb[2]) {
            hsv[0] = 2 + (rgb[2] - rgb[0]) / del;
        } else {
            hsv[0] = 4 + (rgb[0] - rgb[1]) / del;
        }
        if (hsv[0] < 0) hsv[0] += 6;
    } else {
        hsv[0] = 0;
        hsv[1] = 0;
    }
}

void hsvToRgb(const double hsv[3], double rgb[3]) {
    const double j = std::floor(hsv[0]);
    const double f = hsv[0] - j;
    const double v = hsv[2];
    const double p = hsv[2] * (1 - hsv[1]);
    const double q = hsv[2] * (1 - hsv[1] * f);
    const double t = hsv[2] * (1 - hsv[1] * (1 - f));

    switch (static_cast<int>(j) % 6) {
        case 0: rgb[0] = v; rgb[1] = t; rgb[2] = p; break;
        case 1: rgb[0] = q; rgb[1] = v; rgb[2] = p; break;
        case 2: rgb[0] = p; rgb[1] = v; rgb[2] = t; break;
        case 3: rgb[0] = p; rgb[1] = q; rgb[2] = v; break;
        case 4: rgb[0] = t; rgb[1] = p; rgb[2] = v; break;
        case 5: rgb[0] = v; rgb[1] = p; rgb[2] = q; break;
        default: rgb[0] = v; rgb[1] = t; rgb[2] = p; break;
    }
}

void applyHueRotation(const ColorMap& in, double hueRotation, ColorMap& out) {
    ColorMap result;
    for (int i = 0; i < 256; ++i) {
        double rgb[3] = {
            in.entries[i][0] / 255.0,
            in.entries[i][1] / 255.0,
            in.entries[i][2] / 255.0,
        };
        double hsv[3];
        rgbToHsv(rgb, hsv);
        hsv[0] += hueRotation * 6;
        hsvToRgb(hsv, rgb);
        result.entries[i][0] = static_cast<int>(std::lround(rgb[0] * 255));
        result.entries[i][1] = static_cast<int>(std::lround(rgb[1] * 255));
        result.entries[i][2] = static_cast<int>(std::lround(rgb[2] * 255));
        result.entries[i][3] = in.entries[i][3];
    }
    out = result;
}

void rgbBlendBetween(int a, int b, ColorMap& palette) {
    if (a == b) return;
    const double range = b - a;
    for (int channel = 0; channel < 3; ++channel) {
        const double vrange = palette.entries[b % 256][channel] - palette.entries[a % 256][channel];
        double c = palette.entries[a % 256][channel];
        const double v = vrange / range;
        for (int i = a + 1; i < b; ++i) {
            c += v;
            palette.entries[i % 256][channel] = static_cast<int>(std::lround(c));
        }
    }
}

bool parseGradientString(const std::string& gradientText, ColorMap& out) {
    if (gradientText.find('}') == std::string::npos) return false;

    const auto firstNewline = gradientText.find_first_of("\r\n");
    const std::string firstLine =
        firstNewline == std::string::npos ? gradientText : gradientText.substr(0, firstNewline);
    if (firstLine.find('{') == std::string::npos) return false;

    const std::vector<std::string> tokens = tokenize(gradientText);

    std::vector<int> indices;
    std::vector<int64_t> colors;

    for (const auto& token : tokens) {
        const std::string lower = toLower(token);
        if (lower.find('}') != std::string::npos || lower.find("opacity:") != std::string::npos) break;
        if (lower.find("index=") != std::string::npos) {
            try {
                indices.push_back(std::stoi(valueAfterEquals(token)));
            } catch (const std::exception&) {
                return false;
            }
        } else if (lower.find("color=") != std::string::npos) {
            try {
                colors.push_back(std::stoll(valueAfterEquals(token)));
            } catch (const std::exception&) {
                // Matches the original's silent-skip behavior for a
                // malformed color value on an otherwise-valid index.
            }
        }
    }

    if (indices.empty()) return false;

    ColorMap palette{};

    std::vector<int> normalizedIndices(indices.size());
    for (size_t k = 0; k < indices.size(); ++k) {
        int index = indices[k];
        while (index < 0) index += 400;
        index = static_cast<int>(std::lround(index * (255.0 / 399.0)));
        if (index < 0 || index >= 256) return false;
        normalizedIndices[k] = index;

        if (k < colors.size()) {
            const int64_t colorVal = colors[k];
            palette.entries[index][0] = static_cast<int>(((colorVal % 256) + 256) % 256);
            palette.entries[index][1] = static_cast<int>(((colorVal / 256) % 256 + 256) % 256);
            palette.entries[index][2] = static_cast<int>(colorVal / 65536);
        }
    }

    for (size_t k = 1; k < normalizedIndices.size(); ++k) {
        rgbBlendBetween(normalizedIndices[k - 1], normalizedIndices[k], palette);
    }
    if (normalizedIndices.size() >= 2 &&
        (normalizedIndices.front() != 0 || normalizedIndices.back() != 255)) {
        rgbBlendBetween(normalizedIndices.back(), normalizedIndices.front() + 256, palette);
    }

    out = palette;
    return true;
}

} // namespace apo
