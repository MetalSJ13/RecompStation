#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>

#include "core/PlatformManager.h"
#include "core/LibraryScanner.h"
#include "database/DatabaseManager.h"
#include "metadata/TheGamesDbClient.h"
#include "metadata/MetadataManager.h"

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
        platformManager.dataDirectory() + "/database/library.sqlite";
    if (!databaseManager.initialize(databasePath)) {
        qCritical() << "Failed to initialize database:" << databaseManager.lastError();
        return 1;
    }

    LibraryScanner libraryScanner(&platformManager, &databaseManager);
    TheGamesDbClient metadataProvider;
    MetadataManager metadataManager(&metadataProvider);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("platformManager", &platformManager);
    engine.rootContext()->setContextProperty("databaseManager", &databaseManager);
    engine.rootContext()->setContextProperty("libraryScanner", &libraryScanner);
    engine.rootContext()->setContextProperty("metadataManager", &metadataManager);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("RecompStation", "Main");
    return app.exec();
}
