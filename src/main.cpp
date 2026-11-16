#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "core/Logging.h"
#include "core/PlatformManager.h"
#include "core/SettingsManager.h"
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

    const QString logDirectory = platformManager.dataDirectory() + "/logs";
    if (!RecompLogging::initialize(logDirectory))
        qWarning() << "Could not initialize the persistent log file. Continuing with Qt's default logger.";

    qCInfo(lcApplication) << "Starting RecompStation" << QCoreApplication::applicationVersion();
    qCInfo(lcApplication) << "Application data directory:" << platformManager.dataDirectory();

    SettingsManager settingsManager;
    DatabaseManager databaseManager;
    const QString databasePath =
        platformManager.dataDirectory() + "/database/library.sqlite";
    if (!databaseManager.initialize(databasePath)) {
        qCCritical(lcApplication) << "Failed to initialize database:"
                                  << databaseManager.lastError();
        return 1;
    }
    qCInfo(lcApplication) << "Library database initialized.";

    LibraryScanner libraryScanner(&platformManager, &databaseManager);
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
        qCWarning(lcApplication) << "Project catalog unavailable:"
                                 << projectCatalog.errorString();
    else
        qCInfo(lcApplication) << "Project catalog loaded.";

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("settingsManager", &settingsManager);
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
                     &app, []() {
        qCCritical(lcUI) << "QML root object creation failed.";
        QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        qCInfo(lcApplication) << "RecompStation is shutting down.";
    });

    engine.loadFromModule("RecompStation", "Main");
    if (engine.rootObjects().isEmpty()) {
        qCCritical(lcUI) << "No QML root object was created.";
        return 1;
    }

    qCInfo(lcUI) << "Main QML interface loaded.";
    // Reconcile after QML is loaded so the library refresh and metadata-scrape
    // connections receive the scan-completed signal.
    libraryScanner.scanAll();
    return app.exec();
}
