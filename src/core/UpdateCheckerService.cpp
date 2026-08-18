#include "UpdateCheckerService.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QRegularExpression>
#include <QStringList>

#ifndef CHERRYGI_VERSION
#define CHERRYGI_VERSION "1.0.0"
#endif

namespace Cherry {

static const char kGitHubReleasesApi[] = "https://api.github.com/repos/sillygru/cherrygi/releases/latest";

UpdateCheckerService::UpdateCheckerService(QObject *parent)
    : QObject(parent)
    , m_currentVersion(QStringLiteral(CHERRYGI_VERSION))
    , m_latestVersion(QStringLiteral(CHERRYGI_VERSION))
    , m_statusMessage(QStringLiteral("Up to date"))
{
}

bool UpdateCheckerService::isVersionNewer(const QString &remoteVer, const QString &currentVer) const
{
    auto cleanVersion = [](QString v) -> QList<int> {
        v = v.trimmed();
        if (v.startsWith('v') || v.startsWith('V')) {
            v = v.mid(1);
        }
        // Extract numeric dot-separated segments
        const QStringList parts = v.split(QRegularExpression(QStringLiteral("[^0-9]+")), Qt::SkipEmptyParts);
        QList<int> numbers;
        for (const auto &p : parts) {
            numbers.append(p.toInt());
        }
        while (numbers.size() < 3) {
            numbers.append(0);
        }
        return numbers;
    };

    const QList<int> remoteNums = cleanVersion(remoteVer);
    const QList<int> currentNums = cleanVersion(currentVer);

    const int compareCount = qMin(remoteNums.size(), currentNums.size());
    for (int i = 0; i < compareCount; ++i) {
        if (remoteNums[i] > currentNums[i]) return true;
        if (remoteNums[i] < currentNums[i]) return false;
    }
    return remoteNums.size() > currentNums.size();
}

void UpdateCheckerService::checkForUpdates(bool userInitiated)
{
    if (m_isChecking) {
        return;
    }

    m_isChecking = true;
    emit checkingChanged();

    m_statusMessage = QStringLiteral("Checking for updates...");
    emit statusMessageChanged();

    QNetworkRequest request(QUrl(QString::fromLatin1(kGitHubReleasesApi)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("User-Agent", (QStringLiteral("cherrygi-update-checker/") + m_currentVersion).toUtf8());
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    QNetworkReply *reply = m_networkManager.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, userInitiated]() {
        m_isChecking = false;
        emit checkingChanged();

        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            handleReleaseResponse(data, userInitiated);
        } else {
            if (userInitiated) {
                m_statusMessage = QStringLiteral("Could not check for updates. Check your connection.");
            } else {
                m_statusMessage = QStringLiteral("Check failed");
            }
            emit statusMessageChanged();
        }

        reply->deleteLater();
    });
}

void UpdateCheckerService::handleReleaseResponse(const QByteArray &data, bool userInitiated)
{
    Q_UNUSED(userInitiated);
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        m_statusMessage = QStringLiteral("Failed to parse release data");
        emit statusMessageChanged();
        return;
    }

    const QJsonObject obj = doc.object();
    const QString tagName = obj.value(QStringLiteral("tag_name")).toString();
    const QString releaseHtmlUrl = obj.value(QStringLiteral("html_url")).toString();
    const QString releaseBody = obj.value(QStringLiteral("body")).toString();

    if (tagName.isEmpty()) {
        m_statusMessage = QStringLiteral("No release found");
        emit statusMessageChanged();
        return;
    }

    m_latestVersion = tagName;
    m_updateUrl = releaseHtmlUrl;
    m_updateNotes = releaseBody;

    const bool newer = isVersionNewer(tagName, m_currentVersion);
    m_isUpdateAvailable = newer;

    if (newer) {
        m_statusMessage = QStringLiteral("Version %1 is available!").arg(tagName);
        emit updateAvailableNotification(m_latestVersion, m_updateUrl);
    } else {
        m_statusMessage = QStringLiteral("CherryGI is up to date (v%1)").arg(m_currentVersion);
    }

    emit updateStateChanged();
    emit statusMessageChanged();
}

} // namespace Cherry
