#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QTimer>
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

    Cherry::AppController appController;
    qmlRegisterSingletonInstance("org.kde.cherrygi", 1, 0, "AppController", &appController);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController", &appController);

    // Register translation domain
    KLocalizedString::setApplicationDomain("cherrygi");

    // Support auto-quit for validation / testing
    int autoQuitSecs = qEnvironmentVariableIntValue("CHERRYGI_AUTO_QUIT");
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLatin1(argv[i]);
        if (arg == "--auto-quit" && i + 1 < argc) {
            autoQuitSecs = QString::fromLatin1(argv[++i]).toInt();
        } else if (arg == "--auto-quit") {
            autoQuitSecs = 5;
        }
    }
    if (autoQuitSecs > 0) {
        QTimer::singleShot(autoQuitSecs * 1000, &app, &QCoreApplication::quit);
    }

    // Load Main QML from module resources
    const QUrl url(QStringLiteral("qrc:/qt/qml/org/kde/cherrygi/src/qml/Main.qml"));
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
