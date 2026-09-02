#pragma once

#include <functional>

#include <QString>

// Help menu's "Check for Updates..." (MainWindow::onCheckForUpdatesTriggered)
// - queries GitHub Releases for the latest apophysis-cpp release and reports
// whether it's newer than the version currently running. Check-only: this
// never downloads or replaces anything itself, it just tells the caller
// where to send the user (releaseUrl) - see install.ps1/install.sh (repo
// root) for the actual download-and-install flow this points at.
namespace apo::ui {

struct UpdateCheckResult {
    // false only when the check itself couldn't complete (no network,
    // GitHub unreachable, unexpected response shape) - errorMessage is only
    // meaningful in that case. Does NOT mean "no update available"; that's
    // updateAvailable below, which is only meaningful when ok is true.
    bool ok = false;
    QString errorMessage;
    bool updateAvailable = false;
    // No leading 'v' (GitHub's tag_name has one; this strips it) - e.g.
    // "1.0.5", matching APO_VERSION's own convention (CMakeLists.txt).
    QString latestVersion;
    // The release's GitHub page (its "html_url"), to open in the user's
    // browser - not a direct asset download link, since which asset is
    // right depends on the OS this is running on, and the release page
    // already lists all of them with the same info install.ps1/.sh use to
    // pick one.
    QString releaseUrl;
};

// Numeric, dot-separated comparison - "1.0.10" > "1.0.9", where a plain
// QString::compare would get that backwards (lexicographic '1' < '9').
// Tolerant of a leading 'v' on either argument and of version strings with
// fewer than 3 components (a missing trailing component counts as 0, so
// "1.2" == "1.2.0") - GitHub tag_name always has the 'v' and 3 components
// per release.yml's tag pattern, but currentVersion is whatever APO_VERSION
// was set to at build time, which a manual/local build could set to
// anything. A component that isn't a valid non-negative integer also counts
// as 0, so a malformed string can't crash this, only compare confusingly -
// checkForUpdates() below never lets a failed *fetch* reach this at all,
// but a syntactically-valid-JSON, semantically-weird tag_name is still
// possible from a hand-triggered workflow_dispatch release.
bool isNewerVersion(const QString& latest, const QString& current);

// Fire-and-forget: owns its own network request and cleans itself up once
// `onResult` has been invoked exactly once, so the caller doesn't need to
// keep anything alive or manage a QNetworkAccessManager's lifetime - just
// call this and let the callback fire whenever the response (or failure)
// arrives. `onResult` is always called on the calling thread's event loop,
// asynchronously (never before checkForUpdates() itself returns).
void checkForUpdates(const QString& currentVersion, std::function<void(UpdateCheckResult)> onResult);

} // namespace apo::ui
