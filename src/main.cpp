#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSet>

#include "core/Logging.h"
#include "core/ControllerManager.h"
#include "core/PlatformManager.h"
#include "core/SettingsManager.h"
#include "core/LauncherDownloadManager.h"
#include "core/GameLaunchManager.h"
#include "core/LibraryScanner.h"
#include "core/ArtworkManager.h"
#include "core/ScreenshotManager.h"
#include "core/LibraryMetadataMatcher.h"
#include "database/DatabaseManager.h"
#include "metadata/TheGamesDbClient.h"
#include "metadata/MetadataManager.h"
#include "github/GitHubManager.h"
#include "projects/ProjectCatalog.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Keep the launcher terminal attached to the GUI process and make the
    // expected desktop-window close behavior explicit.
    QGuiApplication::setQuitOnLastWindowClosed(true);
    QObject::connect(&app, &QGuiApplication::lastWindowClosed,
                     &app, &QCoreApplication::quit);
    QGuiApplication::setOrganizationName("RecompStation");
    QGuiApplication::setApplicationName("RecompStation");
    QGuiApplication::setApplicationVersion("0.1.0");

    ProjectCatalog projectCatalog;
    if (!projectCatalog.isLoaded())
        qWarning() << "Project catalog unavailable:" << projectCatalog.errorString();

    PlatformManager platformManager;
    if (!platformManager.initialize(projectCatalog.supportedPlatforms())) {
        qCritical() << "Failed to initialize application directories.";
        return 1;
    }

    const QString logDirectory = platformManager.dataDirectory() + "/logs";
    if (!RecompLogging::initialize(logDirectory))
        qWarning() << "Could not initialize the persistent log file. Continuing with Qt's default logger.";

    qCInfo(lcApplication) << "Starting RecompStation" << QCoreApplication::applicationVersion();
    qCInfo(lcApplication) << "Application data directory:" << platformManager.dataDirectory();

    SettingsManager settingsManager(platformManager.settingsFilePath());
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
    metadataManager.setProviderEnabled(settingsManager.theGamesDbEnabled());
    QObject::connect(&settingsManager, &SettingsManager::theGamesDbEnabledChanged,
                     &metadataManager, [&settingsManager, &metadataManager]() {
        metadataManager.setProviderEnabled(settingsManager.theGamesDbEnabled());
    });
    ArtworkManager artworkManager(&databaseManager,
                                  platformManager.dataDirectory() + "/artwork");
    ScreenshotManager screenshotManager(&databaseManager,
                                        platformManager.dataDirectory() + "/artwork",
                                        qEnvironmentVariable("RECOMPSTATION_TGDB_API_KEY"));
    LibraryMetadataMatcher metadataMatcher(&databaseManager, &metadataManager,
                                           &artworkManager);
    GitHubManager githubManager;
    LauncherDownloadManager launcherDownloadManager(&platformManager);
    GameLaunchManager gameLaunchManager(&launcherDownloadManager);
    ControllerManager controllerManager;

    // Do not let RecompStation consume controller input while a launched game
    // is active. Keep input suspended until every launched process has exited.
    QSet<QString> activeGameProcesses;
    QObject::connect(&gameLaunchManager, &GameLaunchManager::launchStarted,
                     &app, [&controllerManager, &activeGameProcesses](const QString &projectId, qint64) {
        activeGameProcesses.insert(projectId);
        controllerManager.setInputEnabled(false);
    });
    QObject::connect(&gameLaunchManager, &GameLaunchManager::launchFailed,
                     &app, [&controllerManager, &activeGameProcesses](const QString &projectId, const QString &) {
        activeGameProcesses.remove(projectId);
        controllerManager.setInputEnabled(activeGameProcesses.isEmpty());
    });
    QObject::connect(&gameLaunchManager, &GameLaunchManager::processFinished,
                     &app, [&controllerManager, &activeGameProcesses](const QString &projectId, int, bool) {
        activeGameProcesses.remove(projectId);
        controllerManager.setInputEnabled(activeGameProcesses.isEmpty());
    });
    if (projectCatalog.isLoaded())
        qCInfo(lcApplication) << "Project catalog loaded with supported platforms:"
                              << projectCatalog.supportedPlatforms();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("settingsManager", &settingsManager);
    engine.rootContext()->setContextProperty("platformManager", &platformManager);
    engine.rootContext()->setContextProperty("databaseManager", &databaseManager);
    engine.rootContext()->setContextProperty("libraryScanner", &libraryScanner);
    engine.rootContext()->setContextProperty("metadataManager", &metadataManager);
    engine.rootContext()->setContextProperty("artworkManager", &artworkManager);
    engine.rootContext()->setContextProperty("screenshotManager", &screenshotManager);
    engine.rootContext()->setContextProperty("metadataMatcher", &metadataMatcher);
    engine.rootContext()->setContextProperty("githubManager", &githubManager);
    engine.rootContext()->setContextProperty("launcherDownloadManager", &launcherDownloadManager);
    engine.rootContext()->setContextProperty("gameLaunchManager", &gameLaunchManager);
    engine.rootContext()->setContextProperty("controllerManager", &controllerManager);
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
    if (settingsManager.scanOnStartup() && platformManager.libraryConfigured())
        libraryScanner.scanAll();
    else if (!platformManager.libraryConfigured())
        qCInfo(lcApplication) << "Library setup is required before the first scan.";
    else
        qCInfo(lcApplication) << "Startup library scan disabled in Settings.";
    return app.exec();
}
