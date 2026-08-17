#include "GitHubAvatarService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#if defined(Q_OS_LINUX)
#include <sys/prctl.h>
#endif
#if defined(Q_OS_UNIX)
#include <unistd.h>
#include <csignal>
#endif

namespace Cherry {

GitHubAvatarService::GitHubAvatarService(QObject *parent)
    : QObject(parent)
{
    connect(&m_ghAuthProcess, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (m_ghAuthInProgress) finishGitHubAuth();
    });
    connect(&m_ghAuthProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_ghAuthInProgress) {
            cancelOperations();
            finishGitHubAuth();
        }
    });
}

GitHubAvatarService::~GitHubAvatarService()
{
    cancelOperations();
}

void GitHubAvatarService::cancelOperations()
{
    if (m_ghAuthProcess.state() != QProcess::NotRunning) {
#if defined(Q_OS_UNIX)
        const qint64 pid = m_ghAuthProcess.processId();
        if (pid > 0) {
            ::kill(-static_cast<pid_t>(pid), SIGKILL);
            ::kill(static_cast<pid_t>(pid), SIGKILL);
        }
#endif
        m_ghAuthProcess.kill();
        m_ghAuthProcess.waitForFinished(100);
    }
}

QString GitHubAvatarService::normalize(const QString &value)
{
    return value.trimmed().toCaseFolded();
}

QString GitHubAvatarService::githubMentionablesUrl(const QString &remoteUrl)
{
    const QString cleanUrl = remoteUrl.trimmed();
    if (cleanUrl.isEmpty()) return QString();

    QString path;
    if (cleanUrl.startsWith(QStringLiteral("git@github.com:"), Qt::CaseInsensitive)) {
        path = cleanUrl.mid(QStringLiteral("git@github.com:").size());
    } else {
        const QUrl parsed(cleanUrl);
        if (parsed.host().compare(QStringLiteral("github.com"), Qt::CaseInsensitive) != 0) return QString();
        path = parsed.path();
    }

    const QStringList parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.size() < 2) return QString();

    QString owner = parts.at(0).trimmed();
    QString repository = parts.at(1).trimmed();
    if (repository.endsWith(QLatin1String(".git"), Qt::CaseInsensitive)) repository.chop(4);
    if (owner.isEmpty() || repository.isEmpty()) return QString();

    const QString encodedOwner = QString::fromLatin1(QUrl::toPercentEncoding(owner));
    const QString encodedRepository = QString::fromLatin1(QUrl::toPercentEncoding(repository));
    return QStringLiteral("https://api.github.com/repos/%1/%2/mentionables/users?per_page=100")
        .arg(encodedOwner, encodedRepository);
}

void GitHubAvatarService::fetchForRemote(const QString &remoteUrl)
{
    const QString apiUrl = githubMentionablesUrl(remoteUrl);
    if (apiUrl.isEmpty()) return;

    const QUrl url(apiUrl);
    const QString remoteKey = url.path();
    if (m_fetchedRemotes.contains(remoteKey)) return;

    m_fetchedRemotes.insert(remoteKey);
    m_pendingRemotes.insert(remoteKey, apiUrl);

    if (m_ghAuthAttempted) {
        flushPendingRequests();
        return;
    }

    if (QStandardPaths::findExecutable(QStringLiteral("gh")).isEmpty()) {
        m_ghAuthAttempted = true;
        flushPendingRequests();
        return;
    }

    if (!m_ghAuthInProgress) {
        m_ghAuthInProgress = true;
#if defined(Q_OS_LINUX)
        m_ghAuthProcess.setChildProcessModifier([]() {
            ::setpgid(0, 0);
            ::prctl(PR_SET_PDEATHSIG, SIGKILL);
        });
#elif defined(Q_OS_UNIX)
        m_ghAuthProcess.setChildProcessModifier([]() {
            ::setpgid(0, 0);
        });
#endif
        m_ghAuthProcess.start(QStringLiteral("gh"), {QStringLiteral("auth"), QStringLiteral("token"), QStringLiteral("--hostname"), QStringLiteral("github.com")});
        QTimer::singleShot(10000, &m_ghAuthProcess, [this]() {
            if (m_ghAuthInProgress && m_ghAuthProcess.state() != QProcess::NotRunning) {
                cancelOperations();
            }
        });
    }
}

void GitHubAvatarService::finishGitHubAuth()
{
    if (!m_ghAuthInProgress) return;
    m_ghAuthInProgress = false;
    m_ghAuthAttempted = true;

    if (m_ghAuthProcess.exitStatus() == QProcess::NormalExit && m_ghAuthProcess.exitCode() == 0) {
        m_ghToken = QString::fromUtf8(m_ghAuthProcess.readAllStandardOutput()).trimmed();
    }
    m_ghAuthProcess.close();
    flushPendingRequests();
}

void GitHubAvatarService::flushPendingRequests()
{
    const auto pending = m_pendingRemotes;
    m_pendingRemotes.clear();
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        fetchPage(it.value(), it.key(), !m_ghToken.isEmpty());
    }
}

void GitHubAvatarService::fetchPage(const QString &url, const QString &remoteKey, bool authenticated)
{
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("cherrygi"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    if (authenticated && !m_ghToken.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + m_ghToken.toUtf8());
    }

    QNetworkReply *reply = m_network.get(request);
    // Avatar metadata is optional; never let an unavailable GitHub endpoint
    // keep a request alive indefinitely or delay the rest of the UI.
    QTimer::singleShot(15000, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, remoteKey, url, authenticated]() {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 401 && authenticated) {
            m_ghToken.clear();
            reply->deleteLater();
            fetchPage(url, remoteKey, false);
            return;
        }
        if (reply->error() != QNetworkReply::NoError || !reply->isOpen() || !reply->isReadable()) {
            m_fetchedRemotes.remove(remoteKey);
            reply->deleteLater();
            return;
        }

        const QByteArray payload = reply->readAll();
        const QString nextUrl = QString::fromUtf8(reply->rawHeader("Link"))
            .split(',', Qt::SkipEmptyParts)
            .value(0)
            .trimmed();
        static const QRegularExpression nextLinkRegex(QStringLiteral(R"(<([^>]+)>;\s*rel="next")"));
        const auto linkMatch = nextLinkRegex.match(nextUrl);

        const QJsonDocument document = QJsonDocument::fromJson(payload);
        if (!document.isArray()) {
            m_fetchedRemotes.remove(remoteKey);
            reply->deleteLater();
            return;
        }

        bool changed = false;
        for (const QJsonValue &value : document.array()) {
            const QJsonObject object = value.toObject();
            const QString before = avatarFor(object.value(QStringLiteral("name")).toString(),
                                             object.value(QStringLiteral("email")).toString());
            storeAvatar(object.value(QStringLiteral("name")).toString(),
                        object.value(QStringLiteral("email")).toString(),
                        object.value(QStringLiteral("login")).toString(),
                        object.value(QStringLiteral("avatar_url")).toString());
            const QString after = avatarFor(object.value(QStringLiteral("name")).toString(),
                                            object.value(QStringLiteral("email")).toString());
            changed = changed || before != after;
        }

        if (changed) emit avatarsChanged();

        const QString next = linkMatch.hasMatch() ? linkMatch.captured(1) : QString();
        reply->deleteLater();
        if (!next.isEmpty()) {
            fetchPage(next, remoteKey, authenticated);
        }
    });
}

void GitHubAvatarService::storeAvatar(const QString &name, const QString &email, const QString &login, const QString &avatarUrl)
{
    if (avatarUrl.trimmed().isEmpty()) return;

    const QString normalizedEmail = normalize(email);
    const QString normalizedName = normalize(name);
    const QString normalizedLogin = normalize(login);
    if (!normalizedEmail.isEmpty()) m_avatars.insert(normalizedEmail, avatarUrl);
    if (!normalizedName.isEmpty()) m_avatars.insert(normalizedName, avatarUrl);
    if (!normalizedLogin.isEmpty()) m_avatars.insert(normalizedLogin, avatarUrl);
}

QString GitHubAvatarService::avatarFor(const QString &authorName, const QString &authorEmail) const
{
    const QString email = normalize(authorEmail);
    if (!email.isEmpty() && m_avatars.contains(email)) return m_avatars.value(email);

    const QString name = normalize(authorName);
    if (!name.isEmpty() && m_avatars.contains(name)) return m_avatars.value(name);
    return QString();
}

} // namespace Cherry
