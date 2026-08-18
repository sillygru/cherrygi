#pragma once

#include <QObject>
#include <QString>
#include <QDate>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace Cherry {

class TelemetryService : public QObject {
    Q_OBJECT

public:
    explicit TelemetryService(QObject *parent = nullptr);
    ~TelemetryService() override = default;

    void start();
    void sendDailyPing();

    QString installationUuid() const { return m_uuid; }

private:
    void ensureUuid();
    QString computeSignature(const QString &dateStr) const;
    QString detectOperatingSystem() const;

    QString m_uuid;
    QString m_lastSentUtcDate;
    QNetworkAccessManager m_networkManager;
    QTimer m_dailyCheckTimer;
};

} // namespace Cherry
