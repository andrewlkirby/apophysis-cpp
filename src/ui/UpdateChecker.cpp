#include "UpdateChecker.h"

#include <algorithm>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace apo::ui {

namespace {

// Same repo install.ps1/install.sh (repo root) already hardcode their own
// copy of - no shared config file exists for it yet, so this just follows
// their existing precedent rather than inventing a new one.
constexpr const char* kReleasesApiUrl = "https://api.github.com/repos/andrewlkirby/apophysis-cpp/releases/latest";

QString stripLeadingV(const QString& s) {
    if (!s.isEmpty() && (s.front() == QChar('v') || s.front() == QChar('V'))) return s.mid(1);
    return s;
}

} // namespace

bool isNewerVersion(const QString& latest, const QString& current) {
    const QStringList latestParts = stripLeadingV(latest).split(QChar('.'));
    const QStringList currentParts = stripLeadingV(current).split(QChar('.'));
    const int componentCount = std::max(latestParts.size(), currentParts.size());
    for (int i = 0; i < componentCount; ++i) {
        bool latestOk = false, currentOk = false;
        const int latestValue = i < latestParts.size() ? latestParts[i].toInt(&latestOk) : 0;
        const int currentValue = i < currentParts.size() ? currentParts[i].toInt(&currentOk) : 0;
        const int l = latestOk ? latestValue : 0;
        const int c = currentOk ? currentValue : 0;
        if (l != c) return l > c;
    }
    return false;
}

void checkForUpdates(const QString& currentVersion, std::function<void(UpdateCheckResult)> onResult) {
    // No parent - this manager (and the reply it creates) outlives this
    // function call, then deletes itself once the request completes, so it
    // can't be parented to anything with a shorter lifetime (e.g. the
    // triggering QAction/menu, if the window closes mid-request).
    auto* manager = new QNetworkAccessManager();

    QNetworkRequest request{QUrl(QString::fromLatin1(kReleasesApiUrl))};
    // GitHub's REST API rejects requests with no User-Agent (403); Accept
    // pins the response shape to their documented v3 JSON media type rather
    // than whatever the default content negotiation would pick.
    request.setRawHeader("User-Agent", "apophysis-cpp-update-checker");
    request.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = manager->get(request);
    QObject::connect(reply, &QNetworkReply::finished, manager, [reply, manager, currentVersion, onResult]() {
        UpdateCheckResult result;
        if (reply->error() != QNetworkReply::NoError) {
            result.ok = false;
            result.errorMessage = reply->errorString();
        } else {
            const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
            const QString tag = obj.value("tag_name").toString();
            if (tag.isEmpty()) {
                result.ok = false;
                result.errorMessage = "unexpected response from GitHub (no release tag found)";
            } else {
                result.ok = true;
                result.latestVersion = stripLeadingV(tag);
                result.releaseUrl = obj.value("html_url").toString();
                result.updateAvailable = isNewerVersion(result.latestVersion, currentVersion);
            }
        }
        reply->deleteLater();
        manager->deleteLater();
        onResult(result);
    });
}

} // namespace apo::ui
