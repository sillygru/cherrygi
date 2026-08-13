#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <KLocalizedString>
#include <KIconTheme>

#include "core/AppController.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("KDE");
    app.setOrganizationDomain("kde.org");
    app.setApplicationName("cherrygi");
    app.setApplicationDisplayName("cherrygi");
    app.setWindowIcon(QIcon::fromTheme("vcs-branch"));

    QQmlApplicationEngine engine;

    Cherry::AppController appController;
    engine.rootContext()->setContextProperty("appController", &appController);

    // Register translation domain
    KLocalizedString::setApplicationDomain("cherrygi");

    // Load Main QML from module
    const QUrl url(QStringLiteral("qrc:/qt/qml/org/kde/cherrygi/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
