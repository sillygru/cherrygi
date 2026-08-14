#pragma once

#include <QQuickImageProvider>
#include <QImage>

namespace Cherry {

class AppController;

class GitImageProvider : public QQuickImageProvider {
public:
    explicit GitImageProvider(AppController *controller);
    ~GitImageProvider() override = default;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    AppController *m_controller{nullptr};
};

} // namespace Cherry
