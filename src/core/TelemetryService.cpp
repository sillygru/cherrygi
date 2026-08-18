#include "TelemetryService.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUuid>
#include <QDateTime>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QSysInfo>

#ifndef CHERRYGI_VERSION
#define CHERRYGI_VERSION "1.0.0"
#endif

#ifndef CHERRYGI_TELEMETRY_SECRET
#define CHERRYGI_TELEMETRY_SECRET ""
#endif

namespace Cherry {

static const char kTelemetryEndpoint[] = "https://api.gru0.dev/telemetry/api/v1/event";

TelemetryService::TelemetryService(QObject *parent)
    : QObject(parent)
{
    ensureUuid();

    // Check periodically for UTC day rollover (every 15 minutes)
    connect(&m_dailyCheckTimer, &QTimer::timeout, this, [this]() {
        const QString todayUtc = QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate);
        if (m_lastSentUtcDate != todayUtc) {
            sendDailyPing();
        }
    });
    m_dailyCheckTimer.setInterval(15 * 60 * 1000);
}

void TelemetryService::start()
{
    sendDailyPing();
    if (!m_dailyCheckTimer.isActive()) {
        m_dailyCheckTimer.start();
    }
}

void TelemetryService::ensureUuid()
{
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/cherrygi");
    QDir().mkpath(configDir);

    const QString filePath = configDir + QStringLiteral("/telemetry-id");
    QFile file(filePath);

    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString stored = QString::fromUtf8(file.readAll()).trimmed();
        file.close();
        if (!stored.isEmpty()) {
            m_uuid = stored;
            return;
        }
    }

    // Generate once and persist
    m_uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(m_uuid.toUtf8());
        file.close();
    }
}

QString TelemetryService::computeSignature(const QString &dateStr) const
{
    const QString message = QStringLiteral("cherrygi:%1:%2").arg(m_uuid, dateStr);
    const QByteArray secret = QByteArray(CHERRYGI_TELEMETRY_SECRET);
    const QByteArray hash = QMessageAuthenticationCode::hash(
        message.toUtf8(),
        secret,
        QCryptographicHash::Sha256
    );
    return QString::fromUtf8(hash.toHex().toLower());
}

QString TelemetryService::detectOperatingSystem() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#elif defined(Q_OS_IOS)
    return QStringLiteral("ios");
#elif defined(Q_OS_ANDROID)
    return QStringLiteral("android");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#else
    return QStringLiteral("linux");
#endif
}

void TelemetryService::sendDailyPing()
{
    if (m_uuid.isEmpty()) {
        ensureUuid();
    }

    const QString todayUtc = QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate);
    const QString signature = computeSignature(todayUtc);
    const QString osName = detectOperatingSystem();
    const QString version = QStringLiteral(CHERRYGI_VERSION);

    QJsonObject payload;
    payload.insert(QStringLiteral("uuid"), m_uuid);
    payload.insert(QStringLiteral("project_id"), QStringLiteral("cherrygi"));
    payload.insert(QStringLiteral("version"), version);
    payload.insert(QStringLiteral("os"), osName);
    payload.insert(QStringLiteral("sig"), signature);

    const QByteArray jsonData = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkRequest request(QUrl(QString::fromLatin1(kTelemetryEndpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("User-Agent", QString("cherrygi/%1 (%2)").arg(version, osName).toUtf8());

    // Fire and forget
    QNetworkReply *reply = m_networkManager.post(request, jsonData);
    connect(reply, &QNetworkReply::finished, this, [this, reply, todayUtc]() {
        if (reply->error() == QNetworkReply::NoError) {
            m_lastSentUtcDate = todayUtc;
        }
        reply->deleteLater();
    });
}

} // namespace Cherry
