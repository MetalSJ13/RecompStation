
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>

#include "core/PlatformManager.h"
#include "database/DatabaseManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QGuiApplication::setOrganizationName("RecompStation");
    QGuiApplication::setApplicationName("RecompStation");
    QGuiApplication::setApplicationVersion("0.1.0");

    PlatformManager platformManager;

    if (!platformManager.initialize()) {
        qCritical() << "Failed to initialize application directories.";
        return 1;
    }

    DatabaseManager databaseManager;

    const QString databasePath =
    platformManager.dataDirectory() +
    "/database/library.sqlite";

        if (!databaseManager.initialize(databasePath)) {
            qCritical() << "Failed to initialize database:"
            << databaseManager.lastError();
            return 1;
        }

        QQmlApplicationEngine engine;

        engine.rootContext()->setContextProperty(
            "platformManager", &platformManager
        );

        engine.rootContext()->setContextProperty(
            "databaseManager", &databaseManager
        );

        QObject::connect(
            &engine,
            &QQmlApplicationEngine::objectCreationFailed,
            &app,
            []() {
                QCoreApplication::exit(-1);
            },
            Qt::QueuedConnection
        );

        engine.loadFromModule("RecompStation", "Main");

        return app.exec();
}
