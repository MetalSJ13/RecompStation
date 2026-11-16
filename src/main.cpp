#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>

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

namespace {
QFile *g_runtimeLogFile = nullptr;
QMutex g_logMutex;

void runtimeMessageHandler(QtMsgType type, const QMessageLogContext &context,
                           const QString &message)
{
    static const char *levelNames[] = {"DEBUG", "INFO", "WARNING", "CRITICAL", "FATAL"};
    const int level = static_cast<int>(type);
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const QString category = context.category ? QString::fromUtf8(context.category)
                                              : QStringLiteral("default");
    const QString line = QStringLiteral("%1 [%2] [%3] %4")
                             .arg(timestamp,
                                  QString::fromLatin1(levelNames[qBound(0, level, 4)]),
                                  category, message);

    QMutexLocker locker(&g_logMutex);
    fprintf(stderr, "%s\\n", line.toLocal8Bit().constData());
    if (g_runtimeLogFile && g_runtimeLogFile->isOpen()) {
        g_runtimeLogFile->write(line.toUtf8());
        g_runtimeLogFile->write("\\n");
        g_runtimeLogFile->flush();
    }

    if (type == QtFatalMsg)
        abort();
}
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("RecompStation");
    QGuiApplication::setApplicationName("RecompStation");
    QGuiApplication::setApplicationVersion("0.1.0");

    SettingsManager settingsManager;

    PlatformManager platformManager;
    if (!platformManager.initialize()) {
        qCritical() << "Failed to initialize application directories.";
        return 1;
    }

    const QString logDirectory = platformManager.dataDirectory() + "/logs";
    QDir().mkpath(logDirectory);
    g_runtimeLogFile = new QFile(logDirectory + "/runtime.log");
    if (g_runtimeLogFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(runtimeMessageHandler);
        qInfo() << "Runtime logging enabled. Log file:" << g_runtimeLogFile->fileName();
    } else {
        qWarning() << "Could not open runtime log file:" << g_runtimeLogFile->fileName();
    }

    SettingsManager settingsManager;    DatabaseManager databaseManager;
    const QString databasePath =
        platformManager.dataDirectory() + "/database/library.sqlite";
    if (!databaseManager.initialize(databasePath)) {
        qCritical() << "Failed to initialize database:" << databaseManager.lastError();
        return 1;
    }

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
        qWarning() << "Project catalog unavailable:" << projectCatalog.errorString();
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
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("RecompStation", "Main");
    // Reconcile after QML is loaded so the library refresh and metadata-scrape
    // connections receive the scan-completed signal.
    libraryScanner.scanAll();
    return app.exec();
}
