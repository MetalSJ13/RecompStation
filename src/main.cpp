#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDateTime>
#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QSysInfo>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#include <QQuickWindow>
#include <QSGRendererInterface>

#include "core/Logging.h"
#include "core/ControllerManager.h"
#include "core/UiSoundPlayer.h"
#include "core/PlatformManager.h"
#include "core/SettingsManager.h"
#include "core/LauncherDownloadManager.h"
#include "core/AppUpdater.h"
#include "core/WineSupport.h"
#include "core/GameLaunchManager.h"
#include "core/LibraryScanner.h"
#include "core/ArtworkManager.h"
#include "core/LaunchBoxArtworkManager.h"
#include "core/ScreenshotManager.h"
#include "core/StorageUsage.h"
#include "core/GameImporter.h"
#include "core/LibraryMetadataMatcher.h"
#include "database/DatabaseManager.h"
#include "metadata/TheGamesDbAllowance.h"
#include "metadata/TheGamesDbClient.h"
#include "metadata/MetadataManager.h"
#include "metadata/LaunchBoxCatalogManager.h"
#include "github/GitHubManager.h"
#include "projects/ProjectCatalog.h"

#ifdef Q_OS_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
#ifndef RECOMPSTATION_BUILD_ID
#define RECOMPSTATION_BUILD_ID "unknown"
#endif
#ifndef RECOMPSTATION_VERSION
#define RECOMPSTATION_VERSION "0.0.0"
#endif

QString onOff(bool value)
{
    return value ? QStringLiteral("on") : QStringLiteral("off");
}

// The block at the top of every session in runtime.log: what someone
// reading a bug report needs before the first event line.
QStringList sessionHeader(const PlatformManager &platformManager,
                          const SettingsManager &settingsManager,
                          const DatabaseManager &databaseManager,
                          const LauncherDownloadManager &launcherDownloadManager,
                          const ProjectCatalog &projectCatalog,
                          const QVariantMap &interfaceScaleInfo,
                          const QString &interfaceScaleSetting)
{
    QStringList lines;
    const QString rule(72, QLatin1Char('='));
    lines << QString() << rule
          << QStringLiteral("RecompStation %1 (build %2) · session started %3")
                 .arg(QCoreApplication::applicationVersion(),
                      QStringLiteral(RECOMPSTATION_BUILD_ID),
                      QDateTime::currentDateTime().toString(Qt::ISODate));
    lines << QStringLiteral("System:    %1 · kernel %2 · %3 · Qt %4")
                 .arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion(),
                      QSysInfo::currentCpuArchitecture(), QString::fromLatin1(qVersion()));
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    const QString session = qEnvironmentVariable("XDG_SESSION_TYPE");
    lines << QStringLiteral("Desktop:   %1 · %2 session · Qt platform %3%4")
                 .arg(desktop.isEmpty() ? QStringLiteral("unknown") : desktop,
                      session.isEmpty() ? QStringLiteral("unknown") : session,
                      QGuiApplication::platformName(),
                      qEnvironmentVariableIsSet("SteamDeck") ? QStringLiteral(" · Steam Deck") : QString());
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        lines << QStringLiteral("Display:   %1 %2x%3 @ %4 Hz · device pixel ratio %5 · %6 screen(s) · interface scale %7, applied %8 percent")
                     .arg(screen->name())
                     .arg(screen->geometry().width()).arg(screen->geometry().height())
                     .arg(qRound(screen->refreshRate()))
                     .arg(screen->devicePixelRatio())
                     .arg(QGuiApplication::screens().size())
                     .arg(interfaceScaleSetting)
                     .arg(interfaceScaleInfo.value(QStringLiteral("applied")).toInt());
    }
    lines << QStringLiteral("Settings:  theme %1 · window %2 · background %3 · reduced motion %4 · metadata %5 first, LaunchBox %6, TheGamesDB %7 (key: %8) · prepare games %9 · fullscreen games %10")
                 .arg(settingsManager.themeName(), settingsManager.displayMode(),
                      settingsManager.libraryBackgroundType(), onOff(settingsManager.reducedMotion()),
                      settingsManager.metadataProviderOrder(), onOff(settingsManager.launchBoxEnabled()),
                      onOff(settingsManager.theGamesDbEnabled()), settingsManager.theGamesDbKeySource(),
                      onOff(settingsManager.prepareGameFiles()))
                 .arg(onOff(settingsManager.startGamesFullscreen()));

    QStringList counts;
    for (const QString &platform : platformManager.platforms())
        counts << QStringLiteral("%1 %2").arg(platform).arg(databaseManager.games(platform).size());
    lines << QStringLiteral("Library:   %1 · %2")
                 .arg(platformManager.libraryConfigured() ? platformManager.libraryDirectory()
                                                          : QStringLiteral("not set up"),
                      counts.join(QStringLiteral(", ")));

    QStringList launchers;
    for (const QVariant &entry : launcherDownloadManager.installedLaunchers())
        launchers << entry.toMap().value(QStringLiteral("projectId")).toString();
    lines << QStringLiteral("Launchers: %1 installed%2 · catalog of %3 projects%4")
                 .arg(launchers.size())
                 .arg(launchers.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(launchers.join(QStringLiteral(", "))))
                 .arg(projectCatalog.projects().size())
                 .arg(projectCatalog.isLoaded() ? QString()
                                               : QStringLiteral(" (failed to load: %1)").arg(projectCatalog.errorString()));
    lines << QStringLiteral("Data:      %1 · log %2 · detailed logging %3")
                 .arg(platformManager.dataDirectory(), RecompLogging::logFilePath(),
                      onOff(RecompLogging::detailedLogging()));
    lines << rule;
    return lines;
}

// What OpenGL this computer offers, checked before any window exists.
//  - none: no usable OpenGL (a VM without 3D, a broken driver). Qt Quick
//    would fail to draw, so RecompStation uses Qt's software renderer.
//  - software: OpenGL drawn by the CPU (Mesa llvmpipe or softpipe, common in
//    VMs); it works, but the live background is kept light.
//  - hardware: a real GPU.
// The user's own QT_QUICK_BACKEND or QSG_RHI_BACKEND is left alone.
QString probeOpenGL()
{
    if (qEnvironmentVariableIsSet("QT_QUICK_BACKEND") || qEnvironmentVariableIsSet("QSG_RHI_BACKEND"))
        return QStringLiteral("hardware");
    QOpenGLContext context;
    QOffscreenSurface surface;
    surface.create();
    if (!context.create() || !surface.isValid() || !context.makeCurrent(&surface))
        return QStringLiteral("none");
    const auto *renderer = reinterpret_cast<const char *>(
        context.functions()->glGetString(GL_RENDERER));
    const QString name = QString::fromLatin1(renderer ? renderer : "").toLower();
    context.doneCurrent();
    return name.contains(QStringLiteral("llvmpipe")) || name.contains(QStringLiteral("softpipe"))
            || name.contains(QStringLiteral("swrast"))
        ? QStringLiteral("software") : QStringLiteral("hardware");
}

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
#ifdef Q_OS_WIN
    // Settings in files on Windows too (not the registry), like on Linux.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    // The controls look the same as on Linux, where Qt uses Fusion; Qt's
    // own Windows style draws text fields as white boxes on the dark theme.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
#endif
    QCoreApplication::setOrganizationName(QStringLiteral("RecompStation"));
    QCoreApplication::setApplicationName(QStringLiteral("RecompStation"));
    scale.settingsFilePath = QDir(PlatformManager::defaultDataDirectory())
        .filePath(QStringLiteral("config/settings.ini"));
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

// program: what to start instead of this program (an updated portable
// folder, whose files have just been swapped in under the same path).
void relaunchApplication(const QString &settingsSection, bool automaticScaleRelaunch,
                         const QString &program = QString())
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
    // Inside an AppImage the program runs from a temporary mount that goes
    // away when this process exits, so restart the AppImage file itself.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    const QString target = !program.isEmpty() ? program
        : !appImage.isEmpty() && QFileInfo(appImage).isExecutable()
        ? appImage : QCoreApplication::applicationFilePath();
    const bool started = QProcess::startDetached(target, arguments);
    qunsetenv("RECOMPSTATION_SCALE_RELAUNCHED");
    if (!started)
        qWarning() << "Could not restart RecompStation.";
}
}

// The Windows version running under Wine on Linux: Wine's ntdll has
// wine_get_version, Windows' doesn't. The Linux version is the better fit.
static bool runningUnderWine()
{
#ifdef Q_OS_WIN
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
#else
    return false;
#endif
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
    QGuiApplication::setApplicationVersion(QStringLiteral(RECOMPSTATION_VERSION));
    // Matches the installed desktop entry, so Wayland desktops show the
    // right icon and name in the taskbar.
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.MetalSJ13.RecompStation"));

    // Window and taskbar icon (the app icon: pixel d-pad on a dark tile).
    QIcon applicationIcon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        applicationIcon.addFile(
            QStringLiteral(":/qt/qml/RecompStation/assets/logo/png/recompstation-icon-%1.png").arg(size),
            QSize(size, size));
    }
    QGuiApplication::setWindowIcon(applicationIcon);

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

    const QString logDirectory = platformManager.dataDirectory() + "/logs";
    if (!RecompLogging::initialize(logDirectory))
        qWarning() << "Could not initialize the persistent log file. Continuing with Qt's default logger.";

    QElapsedTimer sessionTimer;
    sessionTimer.start();

    // RecompStation's own updates. A portable-zip update staged before an
    // unexpected exit is put in place now, and this start hands over to it;
    // otherwise what a finished update left behind is removed.
    AppUpdater appUpdater(platformManager.dataDirectory() + "/downloads/app-update");
    const QString portableProgram = appUpdater.installKind() == QStringLiteral("zip")
        ? QDir(QCoreApplication::applicationDirPath()).canonicalPath() + QStringLiteral("/RecompStationApp")
        : QString();
    if (appUpdater.finishAtStartup()) {
        relaunchApplication(QString(), false, portableProgram);
        return 0;
    }
    QObject::connect(&appUpdater, &AppUpdater::restartRequested, &app,
                     [&app, &appUpdater, portableProgram]() {
        qCInfo(lcApplication) << "Restarting into the updated version.";
        appUpdater.applyStagedUpdate();
        relaunchApplication(QString(), false, portableProgram);
        app.quit();
    });
    // "Later" on a portable update: it is put in place when RecompStation quits.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &appUpdater,
                     [&appUpdater]() { appUpdater.applyStagedUpdate(); });

    SettingsManager settingsManager(platformManager.settingsFilePath());
    if (settingsManager.detailedLogging())
        RecompLogging::setDetailedLogging(true);
    QObject::connect(&settingsManager, &SettingsManager::detailedLoggingChanged, &app,
                     [&settingsManager]() {
        RecompLogging::setDetailedLogging(settingsManager.detailedLogging());
    });

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
        qCDebug(lcApplication) << "Interface scale:" << interfaceScale.setting
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
                     &app, [&app, &appUpdater, portableProgram](const QString &section) {
        qCInfo(lcApplication) << "Restart requested from Settings:" << section;
        // A staged portable update goes in first, so the restart opens it.
        const bool updated = appUpdater.applyStagedUpdate();
        relaunchApplication(section, false, updated ? portableProgram : QString());
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

    LibraryScanner libraryScanner(&platformManager, &databaseManager);
    TheGamesDbClient metadataProvider(settingsManager.theGamesDbApiKey());
    // Saving requests: the platform list is reused from disk for 30 days,
    // and the reported allowance pauses automatic lookups when it is low.
    TheGamesDbAllowance theGamesDbAllowance(
        platformManager.dataDirectory() + "/metadata/thegamesdb-allowance.ini");
    metadataProvider.setPlatformCachePath(
        platformManager.dataDirectory() + "/metadata/thegamesdb-platforms.json");
    metadataProvider.setAllowance(&theGamesDbAllowance);
    MetadataManager metadataManager(&metadataProvider, nullptr,
                                    settingsManager.theGamesDbEnabled());
    metadataManager.setAllowance(&theGamesDbAllowance);
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
    screenshotManager.setAllowance(&theGamesDbAllowance);
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
                            &metadataManager, &theGamesDbAllowance]() {
        const QString key = settingsManager.theGamesDbApiKey();
        // A different key has its own allowance.
        theGamesDbAllowance.reset();
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
    // Nothing is downloading yet: finish or undo an update that was cut
    // short, then clear what an interrupted session left.
    launcherDownloadManager.recoverInterruptedUpdates();
    launcherDownloadManager.cleanUpStaging();
    GameLaunchManager gameLaunchManager(&launcherDownloadManager);
    gameLaunchManager.setProjectCatalog(&projectCatalog);
    gameLaunchManager.setSettingsManager(&settingsManager);
    ControllerManager controllerManager;
    UiSoundPlayer uiSounds(&settingsManager);
    // No interface sounds while a game is running.
    QObject::connect(&controllerManager, &ControllerManager::gameSessionActiveChanged,
                     &uiSounds, [&controllerManager, &uiSounds]() {
        uiSounds.setSuppressed(controllerManager.gameSessionActive());
    });
    controllerManager.setSettingsManager(&settingsManager);

    RecompLogging::writeBlock(sessionHeader(platformManager, settingsManager, databaseManager,
                                            launcherDownloadManager, projectCatalog,
                                            interfaceScaleInfo, interfaceScale.setting));

    const QString openGL = probeOpenGL();
    if (openGL == QStringLiteral("none")) {
        qCWarning(lcUI) << "No usable OpenGL; drawing with Qt's software renderer"
                           " (the live background is replaced by a gradient).";
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    } else if (openGL == QStringLiteral("software")) {
        qCInfo(lcUI) << "OpenGL is drawn by the CPU; the live background runs at reduced quality.";
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("softwareGraphics", openGL != QStringLiteral("hardware"));
    WineSupport wineSupport;
    wineSupport.refresh();
    gameLaunchManager.setWineSupport(&wineSupport);
    engine.rootContext()->setContextProperty("wineSupport", &wineSupport);
    engine.rootContext()->setContextProperty("runningUnderWine", runningUnderWine());
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
    engine.rootContext()->setContextProperty("appUpdater", &appUpdater);
    engine.rootContext()->setContextProperty("gameLaunchManager", &gameLaunchManager);
    engine.rootContext()->setContextProperty("controllerManager", &controllerManager);
    engine.rootContext()->setContextProperty("uiSounds", &uiSounds);
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
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&sessionTimer]() {
        const qint64 seconds = sessionTimer.elapsed() / 1000;
        RecompLogging::writeBlock({
            QStringLiteral("==== session ended %1 after %2 h %3 min %4 s ====")
                .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                .arg(seconds / 3600).arg((seconds / 60) % 60).arg(seconds % 60)});
    });

    engine.loadFromModule("RecompStation", "Main");
    if (engine.rootObjects().isEmpty()) {
        qCCritical(lcUI) << "No QML root object was created.";
        return 1;
    }

    qCInfo(lcUI) << "Interface loaded.";
    // Which graphics API Qt Quick ended up with (and, for OpenGL, which
    // driver): the first thing to know about drawing problems.
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
        QObject::connect(window, &QQuickWindow::sceneGraphInitialized, window, [window]() {
            QString api;
            switch (window->rendererInterface()->graphicsApi()) {
            case QSGRendererInterface::OpenGL: api = QStringLiteral("OpenGL"); break;
            case QSGRendererInterface::Vulkan: api = QStringLiteral("Vulkan"); break;
            case QSGRendererInterface::Software: api = QStringLiteral("software"); break;
            default: api = QStringLiteral("other"); break;
            }
            QString driver;
            if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
                QOpenGLFunctions *gl = context->functions();
                driver = QStringLiteral(" · %1 · %2").arg(
                    QString::fromLatin1(reinterpret_cast<const char *>(gl->glGetString(GL_RENDERER))),
                    QString::fromLatin1(reinterpret_cast<const char *>(gl->glGetString(GL_VERSION))));
            }
            qCInfo(lcUI).noquote() << "Renderer:" << api + driver;
        }, Qt::DirectConnection);
    }
    // Reconcile after QML is loaded so the library refresh and metadata-scrape
    // connections receive the scan-completed signal.
    if (settingsManager.scanOnStartup() && platformManager.libraryConfigured())
        libraryScanner.scanAllAsync();
    else if (!platformManager.libraryConfigured())
        qCInfo(lcApplication) << "Library setup is required before the first scan.";
    else
        qCInfo(lcApplication) << "Startup library scan disabled in Settings.";
    appUpdater.checkAtStartup();
    return app.exec();
}
