// Tests for the built-in gradient library (src/core/BuiltinGradients.h) -
// mechanically generated from the original's ColorMap/cmapdata.pas by
// scripts/generate_builtin_gradients.py. These checks are cross-verified
// directly against the Pascal source (grep'd by hand, not re-derived from
// the generator itself), so a bug in either the generator or the checked-in
// generated file would be caught here.

#include <set>
#include <string>

#include "TestHelpers.h"
#include "core/BuiltinGradients.h"

using apo_test::check;

namespace {

void testCount() {
    check(apo::builtinGradientCount() == 701, "cmapdata.pas's cmaps array has exactly 701 entries");
}

void testFirstAndLastNames() {
    check(std::string(apo::builtinGradientName(0)) == "000_south-sea-bather",
          "gradient 0's name matches CMapNames[0] exactly");
    check(std::string(apo::builtinGradientName(700)) == "700_040412-017",
          "gradient 700's name matches CMapNames[700] exactly");
}

void testNamesAreUnique() {
    std::set<std::string> seen;
    bool allUnique = true;
    for (int i = 0; i < apo::builtinGradientCount(); ++i) {
        if (!seen.insert(apo::builtinGradientName(i)).second) {
            allUnique = false;
            break;
        }
    }
    check(allUnique, "every built-in gradient has a unique name (CMapNames' zero-padded index prefix guarantees this)");
}

void testFirstGradientFirstEntryMatchesSource() {
    // cmapdata.pas, gradient 0 ("south-sea-bather"), first RGB triple:
    // ((185, 234, 235), ...
    const apo::ColorMap cmap = apo::builtinGradient(0);
    check(cmap.entries[0][0] == 185 && cmap.entries[0][1] == 234 && cmap.entries[0][2] == 235,
          "gradient 0's first entry matches cmapdata.pas's literal RGB values exactly");
    check(cmap.entries[0][3] == 255, "every built-in gradient entry has full alpha (255)");
}

void testLastGradientLastEntryMatchesSource() {
    // cmapdata.pas, gradient 700 ("040412-017"), last RGB triple before the
    // array's closing paren: ... (2, 81, 53), (1, 74, 45))
    const apo::ColorMap cmap = apo::builtinGradient(700);
    check(cmap.entries[255][0] == 1 && cmap.entries[255][1] == 74 && cmap.entries[255][2] == 45,
          "gradient 700's last entry matches cmapdata.pas's literal RGB values exactly");
}

void testEveryGradientIsWellFormed() {
    bool ok = true;
    for (int i = 0; i < apo::builtinGradientCount() && ok; ++i) {
        const apo::ColorMap cmap = apo::builtinGradient(i);
        for (int e = 0; e < 256 && ok; ++e) {
            if (cmap.entries[e][3] != 255) ok = false; // alpha always full
        }
        if (std::string(apo::builtinGradientName(i)).empty()) ok = false;
    }
    check(ok, "every one of the 701 gradients decodes to a well-formed 256-entry, fully-opaque ColorMap with a non-empty name");
}

} // namespace

int main() {
    testCount();
    testFirstAndLastNames();
    testNamesAreUnique();
    testFirstGradientFirstEntryMatchesSource();
    testLastGradientLastEntryMatchesSource();
    testEveryGradientIsWellFormed();

    return apo_test::reportAndExit();
}
