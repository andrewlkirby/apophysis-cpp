#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../Flame.h"

namespace apo {

// Loads every <flame> in a .flame/.flam3 XML file (a single file can
// contain a whole flame library), matching Flame/ParameterIO.pas - but via
// a real XML parser (pugixml) instead of the original's regex-based
// approach, per the migration plan's Phase 4. Returns an empty vector (not
// an error code/exception) on a file that doesn't parse as valid XML or
// has no <flame> elements.
std::vector<std::unique_ptr<Flame>> loadFlameFile(const std::string& path);

// Writes `flames` to `path` as a .flame XML file. Returns false on any
// I/O failure.
bool saveFlameFile(const std::string& path, const std::vector<const Flame*>& flames);

// Same parsing as loadFlameFile, but from an in-memory XML string instead
// of a file - the system-clipboard Paste path (MainWindow's Edit menu)
// wants this directly, since a clipboard's text was never written to disk.
std::vector<std::unique_ptr<Flame>> loadFlameFromString(const std::string& xml);

// Same serialization as saveFlameFile, but returned as a string instead of
// written to a file - the system-clipboard Copy path wants this directly.
// Matches Main.pas's mnuCopyClick (Trim(FlameToXML(cp, false, false))): a
// single flame's <flame>...</flame> XML, community-sharable as plain text.
std::string saveFlameToString(const std::vector<const Flame*>& flames);

} // namespace apo
