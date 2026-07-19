#pragma once

#include <string>
#include <vector>

#include "../ColorMap.h"

namespace apo {

// Loads a standalone gradient file into a 256-entry ColorMap, matching
// ColorMap/GradientHlpr.pas / cmap.pas. Dispatches on file extension:
//   - .map: a plain 256-line "R G B" text file (one entry per line, 0-255
//     each). This app's own Delphi source only ever *saves* .map files
//     (Forms/Adjust.pas's SaveMap) - no loader for them exists anywhere in
//     the original at all, so there's no prior behavior to match; this
//     port adds loading since it's straightforward and strictly more
//     complete than the app it's replacing.
//   - .ugr (Ultra Fractal format): a file of named, brace-delimited
//     gradient blocks. `gradientName` selects which block to load; if
//     empty, the first block in the file is used. Once the right block is
//     isolated, its content is exactly the same brace-delimited
//     `index=N color=RRGGBB` grammar ColorMap.h's parseGradientString()
//     already implements - so this is mostly "find the named block",
//     not new gradient-parsing logic.
//
// Returns false (leaving `out` unmodified) on a malformed/unreadable file
// or a `gradientName` not found in a .ugr file.
bool loadGradientFile(const std::string& path, ColorMap& out, const std::string& gradientName = "");

// Lists every gradient block name in a .ugr file, in file order. Empty on
// a malformed/unreadable file or a file with no named blocks (including
// any non-.ugr file - this has no meaning for .map, which holds exactly
// one anonymous gradient).
std::vector<std::string> listUgrGradientNames(const std::string& path);

} // namespace apo
