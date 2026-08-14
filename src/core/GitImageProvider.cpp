#include "GitImageProvider.h"
#include "AppController.h"
#include "IGitService.h"
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

namespace Cherry {

GitImageProvider::GitImageProvider(AppController *controller)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_controller(controller)
{
}

QImage GitImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    if (!m_controller || !m_controller->activeService()) {
        if (size) *size = QSize(0, 0);
        return QImage();
    }

    // Example id: "working/old/src/assets/logo.png?t=12345"
    // Extract query parameter if present
    QString cleanId = id;
    int queryIdx = cleanId.indexOf('?');
    if (queryIdx != -1) {
        cleanId = cleanId.left(queryIdx);
    }

    QStringList parts = cleanId.split('/', Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        if (size) *size = QSize(0, 0);
        return QImage();
    }

    QString scope = parts[0]; // "working", "commit", "stash"
    QString targetRef;
    QString filePath;

    if (scope == "working") {
        QString type = parts[1]; // "old" or "new"
        filePath = parts.mid(2).join('/');
        targetRef = (type == "old") ? "HEAD" : "";
    } else if (scope == "commit") {
        QString sha = parts[1];
        QString type = (parts.size() > 2) ? parts[2] : "new";
        filePath = parts.mid(3).join('/');
        targetRef = (type == "old") ? QString("%1~1").arg(sha) : sha;
    } else if (scope == "stash") {
        QString stashId = parts[1];
        QString type = (parts.size() > 2) ? parts[2] : "new";
        filePath = parts.mid(3).join('/');
        targetRef = (type == "old") ? QString("%1^1").arg(stashId) : stashId;
    } else {
        if (size) *size = QSize(0, 0);
        return QImage();
    }

    QByteArray rawData = m_controller->activeService()->getFileBlob(filePath, targetRef);
    if (rawData.isEmpty()) {
        if (size) *size = QSize(0, 0);
        return QImage();
    }

    QImage image;
    image.loadFromData(rawData);

    if (image.isNull()) {
        if (size) *size = QSize(0, 0);
        return QImage();
    }

    if (size) {
        *size = image.size();
    }

    if (requestedSize.isValid() && !requestedSize.isEmpty() && requestedSize != image.size()) {
        return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return image;
}

} // namespace Cherry
