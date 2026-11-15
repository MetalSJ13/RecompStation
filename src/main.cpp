#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>

#include "core/PlatformManager.h"
#include "core/LauncherDownloadManager.h"
#include "core/GameLaunchManager.h"
#include "core/LibraryScanner.h"
#include "core/ArtworkManager.h"
#include "core/LibraryMetadataMatcher.h"
#include "database/DatabaseManager.h"
#include "metadata/TheGamesDbClient.h"
#include "metadata/MetadataManager.h"
#include "github/GitHubManager.h"
#include "projects/ProjectCatalog.h"

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
    // Reconcile the persisted library with the current ROM folders at startup.
    libraryScanner.scanAll();
    TheGamesDbClient metadataProvider;
    MetadataManager metadataManager(&metadataProvider);
    ArtworkManager artworkManager(&databaseManager,
                                  platformManager.dataDirectory() + "/artwork");
    LibraryMetadataMatcher metadataMatcher(&databaseManager, &metadataManager,
                                           &artworkManager);
    GitHubManager githubManager;
    LauncherDownloadManager launcherDownloadManager(&platformManager);
    GameLaunchManager gameLaunchManager(&launcherDownloadManager);
    ProjectCatalog projectCatalog;
    if (!projectCatalog.isLoaded())
        qWarning() << "Project catalog unavailable:" << projectCatalog.errorString();
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("platformManager", &platformManager);
    engine.rootContext()->setContextProperty("databaseManager", &databaseManager);
    engine.rootContext()->setContextProperty("libraryScanner", &libraryScanner);
    engine.rootContext()->setContextProperty("metadataManager", &metadataManager);
    engine.rootContext()->setContextProperty("artworkManager", &artworkManager);
    engine.rootContext()->setContextProperty("metadataMatcher", &metadataMatcher);
    engine.rootContext()->setContextProperty("githubManager", &githubManager);
    engine.rootContext()->setContextProperty("launcherDownloadManager", &launcherDownloadManager);
    engine.rootContext()->setContextProperty("gameLaunchManager", &gameLaunchManager);
    engine.rootContext()->setContextProperty("projectCatalog", &projectCatalog);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("RecompStation", "Main");
    return app.exec();
}
