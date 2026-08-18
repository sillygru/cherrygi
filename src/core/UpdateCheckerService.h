#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace Cherry {

class UpdateCheckerService : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isUpdateAvailable READ isUpdateAvailable NOTIFY updateStateChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateStateChanged)
    Q_PROPERTY(QString updateUrl READ updateUrl NOTIFY updateStateChanged)
    Q_PROPERTY(QString updateNotes READ updateNotes NOTIFY updateStateChanged)
    Q_PROPERTY(bool isChecking READ isChecking NOTIFY checkingChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit UpdateCheckerService(QObject *parent = nullptr);
    ~UpdateCheckerService() override = default;

    bool isUpdateAvailable() const { return m_isUpdateAvailable; }
    QString currentVersion() const { return m_currentVersion; }
    QString latestVersion() const { return m_latestVersion; }
    QString updateUrl() const { return m_updateUrl; }
    QString updateNotes() const { return m_updateNotes; }
    bool isChecking() const { return m_isChecking; }
    QString statusMessage() const { return m_statusMessage; }

    Q_INVOKABLE void checkForUpdates(bool userInitiated = false);

signals:
    void updateStateChanged();
    void checkingChanged();
    void statusMessageChanged();
    void updateAvailableNotification(const QString &latestVersion, const QString &url);

private:
    void handleReleaseResponse(const QByteArray &data, bool userInitiated);
    bool isVersionNewer(const QString &remoteVer, const QString &currentVer) const;

    QString m_currentVersion;
    QString m_latestVersion;
    QString m_updateUrl;
    QString m_updateNotes;
    QString m_statusMessage;
    bool m_isUpdateAvailable{false};
    bool m_isChecking{false};

    QNetworkAccessManager m_networkManager;
};

} // namespace Cherry
