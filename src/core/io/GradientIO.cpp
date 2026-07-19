#include "GradientIO.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <sstream>

namespace apo {
namespace {

std::string toLowerExt(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

std::string extensionOf(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    return toLowerExt(path.substr(dot));
}

// Matches Forms/Adjust.pas's SaveMap format: 256 lines, each "%3d %3d %3d"
// (R G B, 0-255). No loader for this format exists anywhere in the
// original (SaveMap has no counterpart) - this is new functionality, not
// a port, so there's no prior behavior to match exactly. Tolerant of a
// trailing comment on any line (SaveMap itself appends one to line 1) and
// of fewer than 256 lines (repeats the last parsed entry for the
// remainder rather than leaving them black).
bool loadMapFile(const std::string& path, ColorMap& out) {
    std::ifstream file(path);
    if (!file) return false;

    ColorMap cmap{};
    std::string line;
    int count = 0;
    while (count < 256 && std::getline(file, line)) {
        std::istringstream iss(line);
        int r, g, b;
        if (!(iss >> r >> g >> b)) continue; // skip blank/malformed lines rather than failing outright
        cmap.entries[count][0] = std::clamp(r, 0, 255);
        cmap.entries[count][1] = std::clamp(g, 0, 255);
        cmap.entries[count][2] = std::clamp(b, 0, 255);
        cmap.entries[count][3] = 255;
        ++count;
    }
    if (count == 0) return false;
    for (int i = count; i < 256; ++i) cmap.entries[i] = cmap.entries[count - 1];

    out = cmap;
    return true;
}

// Matches ColorMap/cmap.pas's GetGradient exactly (verified directly
// against source): scans lines for one whose trimmed text starts with
// "<gradientName> " (name followed by a space - not a delimited match, so
// a name that's a prefix of another name needs the space to disambiguate),
// then collects that line and every following line up to and including
// the first one containing '}'. The isolated block is exactly
// ColorMap.h's parseGradientString() grammar - no new parsing logic
// needed here, just block extraction.
bool findUgrBlock(const std::vector<std::string>& lines, const std::string& gradientName, std::string& blockText) {
    size_t start = std::string::npos;
    if (gradientName.empty()) {
        for (size_t i = 0; i < lines.size(); ++i) {
            if (!trim(lines[i]).empty()) {
                start = i;
                break;
            }
        }
    } else {
        const std::string prefix = gradientName + " ";
        for (size_t i = 0; i < lines.size(); ++i) {
            if (trim(lines[i]).rfind(prefix, 0) == 0) {
                start = i;
                break;
            }
        }
    }
    if (start == std::string::npos) return false;

    blockText = lines[start];
    size_t i = start;
    while (blockText.find('}') == std::string::npos) {
        ++i;
        if (i >= lines.size()) return false; // never closed
        blockText += '\n';
        blockText += lines[i];
    }
    return true;
}

bool loadUgrFile(const std::string& path, ColorMap& out, const std::string& gradientName) {
    std::ifstream file(path);
    if (!file) return false;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) lines.push_back(line);

    std::string blockText;
    if (!findUgrBlock(lines, gradientName, blockText)) return false;

    return parseGradientString(blockText, out);
}

} // namespace

bool loadGradientFile(const std::string& path, ColorMap& out, const std::string& gradientName) {
    if (extensionOf(path) == ".map") return loadMapFile(path, out);
    return loadUgrFile(path, out, gradientName);
}

std::vector<std::string> listUgrGradientNames(const std::string& path) {
    std::ifstream file(path);
    if (!file) return {};

    std::vector<std::string> names;
    std::string line;
    bool inBlock = false;
    while (std::getline(file, line)) {
        const std::string t = trim(line);
        if (t.empty()) continue;

        if (!inBlock) {
            const size_t sp = t.find(' ');
            names.push_back(sp == std::string::npos ? t : t.substr(0, sp));
            inBlock = t.find('}') == std::string::npos;
        } else if (t.find('}') != std::string::npos) {
            inBlock = false;
        }
    }
    return names;
}

} // namespace apo
