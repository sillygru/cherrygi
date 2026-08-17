#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QIcon>
#include <QDateTime>
#include <KLocalizedString>
#include <KIconTheme>
#include <cstdio>
#include <cstdlib>

#include "core/AppController.h"
#include "core/GitImageProvider.h"

static void cherryMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Filter out known upstream framework or environment noise:
    // 1. Spurious XDG Desktop Portal registration failure when running standalone dev builds
    if (msg.contains(QLatin1String("Failed to register with host portal")) ||
        msg.contains(QLatin1String("App info not found for 'org.kde.cherrygi'"))) {
        return;
    }
    // 2. Internal Qt socket teardown warnings during asynchronous request aborts
    if (msg.contains(QLatin1String("QIODevice::read (QSslSocket): device not open"))) {
        return;
    }
    // 3. Upstream KDE qqc2-desktop-style MenuItem context menu standard key shortcut warnings
    if (msg.contains(QLatin1String("Only binding to one of multiple key bindings associated with"))) {
        return;
    }

    const char *colorCode = "\033[0m";
    const char *levelStr = "INFO";

    switch (type) {
    case QtDebugMsg:
        colorCode = "\033[36m"; // Cyan
        levelStr = "DEBUG";
        break;
    case QtInfoMsg:
        colorCode = "\033[32m"; // Green
        levelStr = "INFO";
        break;
    case QtWarningMsg:
        colorCode = "\033[33;1m"; // Bold Yellow
        levelStr = "WARNING";
        break;
    case QtCriticalMsg:
        colorCode = "\033[31;1m"; // Bold Red
        levelStr = "CRITICAL";
        break;
    case QtFatalMsg:
        colorCode = "\033[35;1m"; // Bold Magenta
        levelStr = "FATAL";
        break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    if (context.file && (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)) {
        fprintf(stderr, "%s[%s] [%s] [%s:%u] %s\033[0m\n",
                colorCode,
                qPrintable(timestamp),
                levelStr,
                context.file,
                context.line,
                qPrintable(msg));
    } else {
        fprintf(stderr, "%s[%s] [%s] %s\033[0m\n",
                colorCode,
                qPrintable(timestamp),
                levelStr,
                qPrintable(msg));
    }
    fflush(stderr);

    if (type == QtFatalMsg) {
        abort();
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(cherryMessageHandler);

    QApplication app(argc, argv);
    app.setOrganizationName("KDE");
    app.setOrganizationDomain("kde.org");
    app.setApplicationName("cherrygi");
    app.setApplicationDisplayName("cherrygi");
    // Match the desktop file ID so KDE/Wayland can associate the window with
    // its launcher entry, while the embedded SVG keeps development builds
    // sharp even before the icon theme is installed.
    app.setDesktopFileName(QStringLiteral("org.kde.cherrygi"));
    // Always use the bundled asset for the running window. A theme lookup can
    // return a stale or unrelated cached icon before the newly installed SVG
    // is picked up by the desktop icon cache.
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/cherrygi.svg")));

    Cherry::AppController appController;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&appController]() {
        appController.cancelAllOperations();
    });
    qmlRegisterSingletonInstance("org.kde.cherrygi", 1, 0, "AppController", &appController);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("gitimage"), new Cherry::GitImageProvider(&appController));
    engine.rootContext()->setContextProperty("appController", &appController);

    // Register translation domain
    KLocalizedString::setApplicationDomain("cherrygi");

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
