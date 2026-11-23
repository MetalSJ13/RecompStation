#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDateTime>
#include <algorithm>
#include <QDir>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QSysInfo>
#include <QStandardPaths>

#include "core/Logging.h"
#include "core/ControllerManager.h"
#include "core/PlatformManager.h"
#include "core/SettingsManager.h"
#include "core/LauncherDownloadManager.h"
#include "core/GameLaunchManager.h"
#include "core/LibraryScanner.h"
#include "core/ArtworkManager.h"
#include "core/LaunchBoxArtworkManager.h"
#include "core/ScreenshotManager.h"
#include "core/StorageUsage.h"
#include "core/GameImporter.h"
#include "core/LibraryMetadataMatcher.h"
#include "database/DatabaseManager.h"
#include "metadata/TheGamesDbClient.h"
#include "metadata/MetadataManager.h"
#include "metadata/LaunchBoxCatalogManager.h"
#include "github/GitHubManager.h"
#include "projects/ProjectCatalog.h"

namespace {
// Interface scale is applied with QT_SCALE_FACTOR, which Qt reads while
// QGuiApplication is constructed. Everything here runs before that.
struct InterfaceScaleStartup {
    bool external = false;     // QT_SCALE_FACTOR came from the user's environment
    bool relaunched = false;   // this process is the automatic re-launch
    QString setting;           // "auto" or a percentage
    int cachedAutoPercent = 0; // auto value measured on the previous run
    int appliedPercent = 100;
    QString settingsFilePath;
};

InterfaceScaleStartup prepareInterfaceScale()
{
    InterfaceScaleStartup scale;
    scale.relaunched = qEnvironmentVariableIsSet("RECOMPSTATION_SCALE_RELAUNCHED");
    qunsetenv("RECOMPSTATION_SCALE_RELAUNCHED");
    scale.external = qEnvironmentVariableIsSet("QT_SCALE_FACTOR");

    // Same location PlatformManager uses: <AppData>/config/settings.ini.
    QCoreApplication::setOrganizationName(QStringLiteral("RecompStation"));
    QCoreApplication::setApplicationName(QStringLiteral("RecompStation"));
    scale.settingsFilePath = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath(QStringLiteral("config/settings.ini"));
    scale.setting = SettingsManager::storedInterfaceScale(scale.settingsFilePath);
    scale.cachedAutoPercent = QSettings(scale.settingsFilePath, QSettings::IniFormat)
        .value(QStringLiteral("display/autoScalePercent"), 0).toInt();

    if (scale.external)
        return scale;
    scale.appliedPercent = scale.setting == QStringLiteral("auto")
        ? (scale.cachedAutoPercent > 0 ? scale.cachedAutoPercent : 100)
        : scale.setting.toInt();
    if (scale.appliedPercent != 100)
        qputenv("QT_SCALE_FACTOR", QByteArray::number(scale.appliedPercent / 100.0));
    return scale;
}

void relaunchApplication(const QString &settingsSection, bool automaticScaleRelaunch)
{
    QStringList arguments = QCoreApplication::arguments().mid(1);
    arguments.erase(std::remove_if(arguments.begin(), arguments.end(),
        [](const QString &argument) {
            return argument.startsWith(QStringLiteral("--settings-section="));
        }), arguments.end());
    if (!settingsSection.isEmpty())
        arguments.append(QStringLiteral("--settings-section=") + settingsSection);
    if (automaticScaleRelaunch)
        qputenv("RECOMPSTATION_SCALE_RELAUNCHED", "1");
    const bool started = QProcess::startDetached(
        QCoreApplication::applicationFilePath(), arguments);
    qunsetenv("RECOMPSTATION_SCALE_RELAUNCHED");
    if (!started)
        qWarning() << "Could not restart RecompStation.";
}
}

int main(int argc, char *argv[])
{
    InterfaceScaleStartup interfaceScale = prepareInterfaceScale();
    QGuiApplication app(argc, argv);
    // Qt has read the factor now. Remove it so launchers and games started
    // from RecompStation are not scaled as well.
    if (!interfaceScale.external)
        qunsetenv("QT_SCALE_FACTOR");
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
    // Library platforms are independent of the curated recompilation catalog.
    // A platform must remain scannable even when no native project exists for it.
    if (!platformManager.initialize()) {
        qCritical() << "Failed to initialize application directories.";
        return 1;
    }
    qInfo() << "Configured library platforms:" << platformManager.platforms();

    const QString logDirectory = platformManager.dataDirectory() + "/logs";
    if (!RecompLogging::initialize(logDirectory))
        qWarning() << "Could not initialize the persistent log file. Continuing with Qt's default logger.";

    qCInfo(lcApplication) << "Starting RecompStation" << QCoreApplication::applicationVersion();
    qCInfo(lcApplication) << "Application data directory:" << platformManager.dataDirectory();

    SettingsManager settingsManager(platformManager.settingsFilePath());

    // Measure the screen in logical pixels (after desktop scaling, before
    // ours) and work out what Auto should be.
    QVariantMap interfaceScaleInfo;
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        const qreal ours = interfaceScale.external ? 1.0 : interfaceScale.appliedPercent / 100.0;
        const QSize logical = screen
            ? QSize(qRound(screen->geometry().width() * ours),
                    qRound(screen->geometry().height() * ours))
            : QSize();
        const int autoPercent = SettingsManager::autoInterfaceScalePercent(logical.height());
        interfaceScaleInfo.insert(QStringLiteral("applied"), interfaceScale.appliedPercent);
        interfaceScaleInfo.insert(QStringLiteral("auto"), autoPercent);
        interfaceScaleInfo.insert(QStringLiteral("external"), interfaceScale.external);
        interfaceScaleInfo.insert(QStringLiteral("environmentValue"),
                                  interfaceScale.external
                                      ? qEnvironmentVariable("QT_SCALE_FACTOR") : QString());
        interfaceScaleInfo.insert(QStringLiteral("screenName"), screen ? screen->name() : QString());
        interfaceScaleInfo.insert(QStringLiteral("logicalWidth"), logical.width());
        interfaceScaleInfo.insert(QStringLiteral("logicalHeight"), logical.height());
        interfaceScaleInfo.insert(QStringLiteral("devicePixelRatio"),
                                  screen ? screen->devicePixelRatio() / ours : 1.0);
        qCInfo(lcApplication) << "Interface scale:" << interfaceScale.setting
                              << "applied" << interfaceScale.appliedPercent << "%"
                              << "auto" << autoPercent << "%"
                              << "screen" << logical << "external" << interfaceScale.external;

        if (!interfaceScale.external && autoPercent != interfaceScale.cachedAutoPercent) {
            QSettings settings(platformManager.settingsFilePath(), QSettings::IniFormat);
            settings.setValue(QStringLiteral("display/autoScalePercent"), autoPercent);
            settings.sync();
        }
        // First run, or the screen changed since the last run: no window has
        // been shown yet, so restart straight away with the right factor.
        if (!interfaceScale.external && !interfaceScale.relaunched
                && settingsManager.interfaceScale() == QStringLiteral("auto")
                && autoPercent != interfaceScale.appliedPercent) {
            qCInfo(lcApplication) << "Restarting to apply automatic interface scale"
                                  << autoPercent << "%";
            relaunchApplication(QString(), true);
            return 0;
        }
    }
    QString startupSettingsSection;
    for (const QString &argument : app.arguments()) {
        if (argument.startsWith(QStringLiteral("--settings-section=")))
            startupSettingsSection = argument.section(QLatin1Char('='), 1);
    }
    QObject::connect(&settingsManager, &SettingsManager::applicationRestartRequested,
                     &app, [&app](const QString &section) {
        qCInfo(lcApplication) << "Restart requested from Settings:" << section;
        relaunchApplication(section, false);
        app.quit();
    });
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
    TheGamesDbClient metadataProvider(settingsManager.theGamesDbApiKey());
    MetadataManager metadataManager(&metadataProvider, nullptr,
                                    settingsManager.theGamesDbEnabled());
    LaunchBoxCatalogManager launchBoxCatalogManager(platformManager.dataDirectory());
    launchBoxCatalogManager.setIndexedPlatforms(platformManager.platforms());
    QObject::connect(&launchBoxCatalogManager, &LaunchBoxCatalogManager::updateCompleted,
                     &settingsManager, [&settingsManager](int, int, int) {
        settingsManager.setLaunchBoxLastUpdated(
            QDateTime::currentDateTime().toString(Qt::ISODate));
    });
    QObject::connect(&settingsManager, &SettingsManager::theGamesDbEnabledChanged,
                     &metadataManager, [&settingsManager, &metadataManager]() {
        metadataManager.setProviderEnabled(settingsManager.theGamesDbEnabled());
    });
    ArtworkManager artworkManager(&databaseManager,
                                  platformManager.dataDirectory() + "/artwork");
    LaunchBoxArtworkManager launchBoxArtworkManager(&databaseManager,
                                                     platformManager.dataDirectory());
    launchBoxArtworkManager.setEnabled(settingsManager.launchBoxEnabled());
    QObject::connect(&settingsManager, &SettingsManager::launchBoxEnabledChanged,
                     &launchBoxArtworkManager, [&settingsManager, &launchBoxArtworkManager]() {
        launchBoxArtworkManager.setEnabled(settingsManager.launchBoxEnabled());
        if (settingsManager.launchBoxEnabled())
            launchBoxArtworkManager.refreshLibraryArtwork();
    });
    QObject::connect(&launchBoxCatalogManager, &LaunchBoxCatalogManager::updateCompleted,
                     &launchBoxArtworkManager, [&launchBoxArtworkManager](int, int, int) {
        launchBoxArtworkManager.refreshLibraryArtwork();
    });
    ScreenshotManager screenshotManager(&databaseManager,
                                        platformManager.dataDirectory() + "/artwork",
                                        settingsManager.theGamesDbApiKey());
    screenshotManager.setProviderEnabled(settingsManager.theGamesDbEnabled());
    QObject::connect(&settingsManager, &SettingsManager::theGamesDbEnabledChanged,
                     &screenshotManager, [&settingsManager, &screenshotManager]() {
        screenshotManager.setProviderEnabled(settingsManager.theGamesDbEnabled());
    });
    LibraryMetadataMatcher metadataMatcher(&databaseManager, &metadataManager,
                                           &artworkManager, &launchBoxCatalogManager,
                                           &settingsManager);
    // A key entered or removed in Settings takes effect immediately.
    QObject::connect(&settingsManager, &SettingsManager::theGamesDbApiKeyChanged,
                     &app, [&settingsManager, &metadataProvider, &screenshotManager,
                            &metadataManager]() {
        const QString key = settingsManager.theGamesDbApiKey();
        metadataProvider.setApiKey(key);
        screenshotManager.setApiKey(key);
        metadataManager.refreshProvider();
    });
    // When LaunchBox becomes usable (enabled, or its catalog installed or
    // updated), give games it may now match another try. LaunchBox-first
    // order keeps this offline for games LaunchBox knows.
    auto rematchWithLaunchBox = [&settingsManager, &launchBoxCatalogManager, &metadataMatcher]() {
        if (settingsManager.launchBoxEnabled() && launchBoxCatalogManager.catalogAvailable()
                && settingsManager.automaticMetadataMatchingEnabled())
            metadataMatcher.scrapeLibrary(true);
    };
    QObject::connect(&launchBoxCatalogManager, &LaunchBoxCatalogManager::updateCompleted,
                     &metadataMatcher, [rematchWithLaunchBox](int, int, int) { rematchWithLaunchBox(); });
    QObject::connect(&settingsManager, &SettingsManager::launchBoxEnabledChanged,
                     &metadataMatcher, rematchWithLaunchBox);
    QObject::connect(&metadataMatcher, &LibraryMetadataMatcher::scrapeFinished,
                     &launchBoxArtworkManager, [&launchBoxArtworkManager](int, int) {
        launchBoxArtworkManager.refreshLibraryArtwork();
    });
    StorageUsage storageUsage(platformManager.dataDirectory());
    GameImporter gameImporter(&platformManager);
    // Newly added games are registered (and matched) by a library scan.
    QObject::connect(&gameImporter, &GameImporter::importFinished,
                     &libraryScanner, [&libraryScanner](int imported, int, int) {
        if (imported > 0)
            libraryScanner.scanAllAsync();
    });
    GitHubManager githubManager;
    LauncherDownloadManager launcherDownloadManager(&platformManager);
    GameLaunchManager gameLaunchManager(&launcherDownloadManager);
    ControllerManager controllerManager;
    controllerManager.setSettingsManager(&settingsManager);

    if (projectCatalog.isLoaded())
        qCInfo(lcApplication) << "Project catalog loaded with supported platforms:"
                              << projectCatalog.supportedPlatforms();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("settingsManager", &settingsManager);
    engine.rootContext()->setContextProperty("platformManager", &platformManager);
    engine.rootContext()->setContextProperty("databaseManager", &databaseManager);
    engine.rootContext()->setContextProperty("libraryScanner", &libraryScanner);
    engine.rootContext()->setContextProperty("metadataManager", &metadataManager);
    engine.rootContext()->setContextProperty("launchBoxCatalogManager", &launchBoxCatalogManager);
    engine.rootContext()->setContextProperty("launchBoxArtworkManager", &launchBoxArtworkManager);
    engine.rootContext()->setContextProperty("artworkManager", &artworkManager);
    engine.rootContext()->setContextProperty("screenshotManager", &screenshotManager);
    engine.rootContext()->setContextProperty("metadataMatcher", &metadataMatcher);
    engine.rootContext()->setContextProperty("githubManager", &githubManager);
    engine.rootContext()->setContextProperty("launcherDownloadManager", &launcherDownloadManager);
    engine.rootContext()->setContextProperty("gameLaunchManager", &gameLaunchManager);
    engine.rootContext()->setContextProperty("controllerManager", &controllerManager);
    engine.rootContext()->setContextProperty("projectCatalog", &projectCatalog);
    engine.rootContext()->setContextProperty("interfaceScaleInfo", interfaceScaleInfo);
    engine.rootContext()->setContextProperty("storageUsage", &storageUsage);
    engine.rootContext()->setContextProperty("gameImporter", &gameImporter);
    engine.rootContext()->setContextProperty("qtVersion", QString::fromLatin1(qVersion()));
    engine.rootContext()->setContextProperty("cpuArchitecture", QSysInfo::currentCpuArchitecture());
    engine.rootContext()->setContextProperty("startupSettingsSection", startupSettingsSection);

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
        libraryScanner.scanAllAsync();
    else if (!platformManager.libraryConfigured())
        qCInfo(lcApplication) << "Library setup is required before the first scan.";
    else
        qCInfo(lcApplication) << "Startup library scan disabled in Settings.";
    return app.exec();
}
