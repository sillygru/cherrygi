#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QObject>

namespace Cherry {

class GitHubAvatarService : public QObject {
    Q_OBJECT

public:
    explicit GitHubAvatarService(QObject *parent = nullptr);
    ~GitHubAvatarService() override;

    void cancelOperations();
    void fetchForRemote(const QString &remoteUrl);
    QString avatarFor(const QString &authorName, const QString &authorEmail) const;
    QHash<QString, QString> avatarOverrides() const { return m_avatars; }

signals:
    void avatarsChanged();

private:
    static QString normalize(const QString &value);
    static QString githubMentionablesUrl(const QString &remoteUrl);

    void fetchPage(const QString &url, const QString &remoteKey, bool authenticated);
    void flushPendingRequests();
    void finishGitHubAuth();
    void storeAvatar(const QString &name, const QString &email, const QString &login, const QString &avatarUrl);

    QNetworkAccessManager m_network;
    QProcess m_ghAuthProcess;
    QSet<QString> m_fetchedRemotes;
    QHash<QString, QString> m_pendingRemotes;
    QHash<QString, QString> m_avatars;
    QString m_ghToken;
    bool m_ghAuthAttempted{false};
    bool m_ghAuthInProgress{false};
};

} // namespace Cherry
