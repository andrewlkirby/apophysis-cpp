// Tests for UpdateChecker.h/.cpp's isNewerVersion() - the pure comparison
// logic behind Help > Check for Updates. Doesn't touch the network at all
// (checkForUpdates() itself isn't exercised here - it's a thin wrapper
// around a real GitHub API call, not something worth mocking out for a
// manually-triggered, check-only menu item); a wrong version *comparison*
// is the actual bug risk (e.g. "1.0.10" sorting before "1.0.9" under a
// naive string compare), so that's what's covered.

#include <QApplication>

#include "../TestHelpers.h"
#include "UpdateChecker.h"

using apo_test::check;
using apo::ui::isNewerVersion;

namespace {

void testBasicNewerOlderEqual() {
    check(isNewerVersion("1.0.5", "1.0.4"), "a higher patch version is newer");
    check(!isNewerVersion("1.0.3", "1.0.4"), "a lower patch version is not newer");
    check(!isNewerVersion("1.0.4", "1.0.4"), "an identical version is not newer");
    check(isNewerVersion("2.0.0", "1.9.9"), "a major bump beats any minor/patch on the other side");
}

void testNumericNotLexicographicComparison() {
    // The actual bug a naive QString::compare would produce - '1' < '9'
    // lexicographically, so "1.0.10" would sort *before* "1.0.9".
    check(isNewerVersion("1.0.10", "1.0.9"), "1.0.10 is newer than 1.0.9 (numeric, not lexicographic, compare)");
    check(!isNewerVersion("1.0.9", "1.0.10"), "and the reverse correctly isn't newer");
}

void testLeadingVIsTolerated() {
    check(isNewerVersion("v1.0.5", "1.0.4"), "a 'v'-prefixed latest version (GitHub's tag_name) compares correctly");
    check(isNewerVersion("1.0.5", "v1.0.4"), "a 'v'-prefixed current version compares correctly too");
    check(!isNewerVersion("v1.0.4", "V1.0.4"), "matching versions differing only by 'v'/'V' prefix are equal");
}

void testMismatchedComponentCountTreatsMissingAsZero() {
    check(!isNewerVersion("1.2", "1.2.0"), "a missing trailing component counts as 0 - 1.2 == 1.2.0");
    check(isNewerVersion("1.2.1", "1.2"), "1.2.1 is newer than 1.2 (== 1.2.0)");
    check(!isNewerVersion("1.2", "1.2.1"), "and 1.2 (== 1.2.0) is not newer than 1.2.1");
}

void testMalformedComponentFallsBackToZeroRatherThanCrashing() {
    // Every apophysis-cpp release tag is a clean "vX.Y.Z" (release.yml's
    // `tags: ['v*.*.*']`), so this is a defensive floor, not a case this
    // should ever hit in practice via a real GitHub response.
    check(!isNewerVersion("1.x.0", "1.0.0"), "a non-numeric component counts as 0, not a crash or a false positive");
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testBasicNewerOlderEqual();
    testNumericNotLexicographicComparison();
    testLeadingVIsTolerated();
    testMismatchedComponentCountTreatsMissingAsZero();
    testMalformedComponentFallsBackToZeroRatherThanCrashing();

    return apo_test::reportAndExit();
}
