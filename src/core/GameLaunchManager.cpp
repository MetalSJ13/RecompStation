#include "GameLaunchManager.h"
#include "WineSupport.h"

#include "LauncherDownloadManager.h"
#include "Logging.h"
#include "SettingsManager.h"
#include "projects/ProjectCatalog.h"

#include <QDir>
#include <QFile>
#include <cstdio>
#include <QFileInfo>
#include <QProcess>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QProcessEnvironment>
#include <QSysInfo>
#include <memory>
#include <QVariantMap>

namespace {
// A launcher's own output (stdout and stderr): written in full to
// logs/launchers/<project>.log (the previous run is kept as .previous.log),
// echoed to the terminal, and the last lines kept in memory so a crash can
// be explained in runtime.log.
struct LauncherOutput
{
    static constexpr int TailLines = 40;
    QFile file;
    QStringList tail;
    QByteArray partial;
    QElapsedTimer timer;

    void open(const QString &projectId, const QString &program)
    {
        timer.start();
        const QString directory = RecompLogging::logDirectory();
        if (directory.isEmpty())
            return;
        const QString folder = QDir(directory).filePath(QStringLiteral("launchers"));
        QDir().mkpath(folder);
        const QString path = QDir(folder).filePath(projectId + QStringLiteral(".log"));
        const QString previous = QDir(folder).filePath(projectId + QStringLiteral(".previous.log"));
        QFile::remove(previous);
        QFile::rename(path, previous);
        file.setFileName(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(RecompLogging::redactHome(
                QStringLiteral("# %1 started %2\n# %3\n")
                    .arg(projectId, QDateTime::currentDateTime().toString(Qt::ISODate), program)).toUtf8());
            file.flush();
        }
    }

    void append(const QByteArray &data)
    {
        fwrite(data.constData(), 1, size_t(data.size()), stderr);
        partial += data;
        int newline;
        while ((newline = partial.indexOf('\n')) >= 0) {
            addLine(QString::fromUtf8(partial.left(newline)));
            partial.remove(0, newline + 1);
        }
        // Very long output without newlines (progress bars) is cut into lines.
        if (partial.size() > 4096) {
            addLine(QString::fromUtf8(partial));
            partial.clear();
        }
    }

    void addLine(QString line)
    {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        line = RecompLogging::redactHome(line);
        if (file.isOpen()) {
            file.write(line.toUtf8());
            file.write("\n");
            file.flush();
        }
        tail.append(line);
        if (tail.size() > TailLines)
            tail.removeFirst();
    }

    void finish()
    {
        if (!partial.isEmpty()) {
            addLine(QString::fromUtf8(partial));
            partial.clear();
        }
        file.close();
    }
};
}

GameLaunchManager::GameLaunchManager(
    LauncherDownloadManager *launcherDownloadManager,
    QObject *parent
)
    : QObject(parent)
    , m_launcherDownloadManager(launcherDownloadManager)
{
    m_preparer.setBackgroundReads(true);
}

GameLaunchManager::~GameLaunchManager()
{
    for (QProcess *process : m_processes) {
        if (process && process->state() != QProcess::NotRunning) {
            process->terminate();
            if (!process->waitForFinished(1500)) {
                process->kill();
                process->waitForFinished(1000);
            }
        }
    }
}

void GameLaunchManager::setProjectCatalog(ProjectCatalog *catalog)
{
    m_projectCatalog = catalog;
}

void GameLaunchManager::setSettingsManager(SettingsManager *settings)
{
    m_settingsManager = settings;
}

void GameLaunchManager::setWineSupport(WineSupport *wine)
{
    m_wine = wine;
}

bool GameLaunchManager::isWindowsLauncher(const QString &projectId) const
{
    if (!m_launcherDownloadManager)
        return false;
    for (const QVariant &entry : m_launcherDownloadManager->installedLaunchers()) {
        const QVariantMap installation = entry.toMap();
        if (installation.value(QStringLiteral("projectId")).toString() != projectId)
            continue;
        return installation.value(QStringLiteral("build")).toString().startsWith(QStringLiteral("windows-"))
            || installation.value(QStringLiteral("executablePath")).toString()
                   .endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive);
    }
    return false;
}

// Wine's %LOCALAPPDATA%: <prefix>/drive_c/users/<user>/AppData/Local, if
// Wine has made it (it does on its first start). The prefix is WINEPREFIX or
// ~/.wine, as with Open Wine's C: drive.
static QString wineLocalAppData(const WineSupport *wine)
{
    if (!wine)
        return {};
    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty())
        user = QDir::home().dirName();
    const QString path = QDir(wine->cDrive()).filePath(QStringLiteral("users/%1/AppData/Local").arg(user));
    return QFileInfo(path).isDir() ? path : QString();
}

static QString wineRoamingAppData(const WineSupport *wine)
{
    const QString local = wineLocalAppData(wine);
    if (local.isEmpty())
        return {};
    const QString path = QDir(local).filePath(QStringLiteral("../Roaming"));
    return QFileInfo(path).isDir() ? QDir::cleanPath(path) : QString();
}

bool GameLaunchManager::wineCanPrepare(const QString &projectId, LaunchPreparer::Options *options) const
{
    if (!m_projectCatalog)
        return false;
    const QString method = m_projectCatalog->project(projectId).value(QStringLiteral("launch")).toMap()
                               .value(QStringLiteral("method")).toString();
    LaunchPreparer::Options result = preparationOptions(projectId);
    result.system = QStringLiteral("windows");
    if (method == QStringLiteral("n64recomp")) {
        result.localAppData = wineLocalAppData(m_wine);
        if (result.localAppData.isEmpty())
            return false; // Wine hasn't made its folders yet: the launcher asks this once.
    } else if (method == QStringLiteral("selectfile") || method == QStringLiteral("fileargument")
               || method == QStringLiteral("discargument") || method == QStringLiteral("placefile")) {
        // The game file's path as Wine shows it to the launcher (Z:\...).
        result.winePaths = true;
        result.appData = wineRoamingAppData(m_wine);
        result.localAppData = wineLocalAppData(m_wine);
        const QVariantMap recipe = m_projectCatalog->project(projectId).value(QStringLiteral("launch")).toMap();
        QVariantList entries = recipe.value(QStringLiteral("selectFiles")).toList();
        if (recipe.contains(QStringLiteral("placeFile")))
            entries.append(recipe.value(QStringLiteral("placeFile")));
        bool needsAppData = false;
        for (const QVariant &item : std::as_const(entries)) {
            const QVariantMap entry = item.toMap();
            if (entry.value(QStringLiteral("folder")).toString() == QStringLiteral("userdata")
                && entry.value(QStringLiteral("windowsBase")).toString() != QStringLiteral("launcher")) {
                const bool local = entry.value(QStringLiteral("windowsBase")).toString() == QStringLiteral("local");
                needsAppData = needsAppData || (local ? result.localAppData.isEmpty() : result.appData.isEmpty());
            }
        }
        if (needsAppData)
            return false; // Wine hasn't made its folders yet: the launcher asks this once.
    } else if (method != QStringLiteral("harbourmasters")) {
        return false; // folders set by variables stay as they are
    }
    if (options)
        *options = result;
    return true;
}

bool GameLaunchManager::runsThroughWine(const QString &projectId) const
{
    return isWindowsLauncher(projectId)
        && ProjectCatalog::systemForKernelType(QSysInfo::kernelType()) != QStringLiteral("windows");
}

QVariantMap GameLaunchManager::wineReadiness(const QString &gamePath) const
{
    const bool hasGame = !gamePath.isEmpty() && QFileInfo(gamePath).isFile();
    QVariantMap result{
        {QStringLiteral("method"), QStringLiteral("wine")},
        {QStringLiteral("gameFile"), hasGame ? gamePath : QString()},
        {QStringLiteral("dataFolder"), QString()},
        {QStringLiteral("savesFolder"), QString()},
        {QStringLiteral("wineDrive"), m_wine ? m_wine->cDrive() : QString()}
    };
    auto set = [&result](const QString &state, const QString &tone,
                         const QString &title, const QString &detail) {
        result.insert(QStringLiteral("state"), state);
        result.insert(QStringLiteral("tone"), tone);
        result.insert(QStringLiteral("title"), title);
        result.insert(QStringLiteral("detail"), detail);
        return result;
    };
    if (!m_settingsManager || !m_settingsManager->windowsLaunchersThroughWine())
        return set(QStringLiteral("wine_off"), QStringLiteral("warn"),
                   QStringLiteral("Turn on Windows launchers through Wine to play"),
                   QStringLiteral("It's off in Settings › Library. The launcher stays installed."));
    if (!m_wine || !m_wine->isAvailable())
        return set(QStringLiteral("wine_missing"), QStringLiteral("warn"),
                   QStringLiteral("Wine isn't installed"),
                   QStringLiteral("Install Wine with your system's package manager to play. The launcher stays installed."));
    if (!hasGame)
        return set(QStringLiteral("no_game"), QStringLiteral("warn"),
                   QStringLiteral("Add your copy of the game"),
                   QStringLiteral("RecompStation doesn't supply games. Add the file from your own copy."));
    return set(QStringLiteral("wine"), QStringLiteral("ok"),
               QStringLiteral("Ready to play through Wine %1").arg(m_wine->version()),
               QStringLiteral("The Windows launcher opens to its own menu, where you pick your game file."));
}

void GameLaunchManager::setPreparationRoots(const QString &homeDirectory, const QString &dataHome)
{
    m_homeDirectory = homeDirectory;
    m_dataHome = dataHome;
}

// The folder a launcher starts in: the folder of its program (an archive
// can keep it in a subfolder), or its install folder for a start script.
QString GameLaunchManager::launcherFolder(const QString &projectId) const
{
    QString executablePath;
    const QString directory = installDirectory(projectId, &executablePath);
    if (executablePath.isEmpty()
        || (!isWindowsLauncher(projectId)
            && QFileInfo(executablePath).suffix().compare(QStringLiteral("sh"), Qt::CaseInsensitive) == 0))
        return directory;
    return QFileInfo(executablePath).absolutePath();
}

QString GameLaunchManager::installDirectory(const QString &projectId, QString *executablePath) const
{
    if (!m_launcherDownloadManager)
        return {};
    for (const QVariant &entry : m_launcherDownloadManager->installedLaunchers()) {
        const QVariantMap installation = entry.toMap();
        if (installation.value(QStringLiteral("projectId")).toString() == projectId) {
            if (executablePath)
                *executablePath = installation.value(QStringLiteral("executablePath")).toString();
            return installation.value(QStringLiteral("directory")).toString();
        }
    }
    return {};
}

LaunchPreparer::Options GameLaunchManager::preparationOptions(const QString &projectId) const
{
    LaunchPreparer::Options options;
    options.homeDirectory = m_homeDirectory;
    options.dataHome = m_dataHome;
    (void)installDirectory(projectId, &options.executablePath);
    if (m_settingsManager) {
        options.prepareFiles = m_settingsManager->prepareGameFiles();
        options.fullscreen = m_settingsManager->startGamesFullscreen();
        options.windowMode = m_settingsManager->launchWindowMode(projectId);
        options.defaultFullscreenApplied = m_settingsManager->defaultFullscreenApplied(projectId);
    }
    return options;
}

QVariantMap GameLaunchManager::launchReadiness(const QString &projectId, const QString &gamePath)
{
    if (!m_projectCatalog)
        return {};
    const QVariantMap project = m_projectCatalog->project(projectId);
    if (project.isEmpty())
        return {};
    LaunchPreparer::Options options = preparationOptions(projectId);
    if (runsThroughWine(projectId)) {
        QVariantMap wine = wineReadiness(gamePath);
        const QString state = wine.value(QStringLiteral("state")).toString();
        if (state != QStringLiteral("wine") || !wineCanPrepare(projectId, &options))
            return wine;
    }
    QVariantMap readiness = m_preparer.readiness(project, gamePath, launcherFolder(projectId), options);
    if (runsThroughWine(projectId) && m_wine) {
        readiness.insert(QStringLiteral("detail"), readiness.value(QStringLiteral("detail")).toString()
                             + QStringLiteral(" Runs through Wine %1.").arg(m_wine->version()));
        readiness.insert(QStringLiteral("wineDrive"), m_wine->cDrive());
    }
    // Read new game files off the UI thread (up to 64 MB each); the answer
    // arrives through launchReadinessChanged().
    for (const QString &path : m_preparer.takePendingReads()) {
        if (m_inspecting.contains(path))
            continue;
        m_inspecting.insert(path);
        auto *watcher = new QFutureWatcher<N64RomInfo>(this);
        connect(watcher, &QFutureWatcher<N64RomInfo>::finished, this, [this, watcher, path]() {
            m_preparer.storeInspection(path, watcher->result());
            m_inspecting.remove(path);
            watcher->deleteLater();
            emit launchReadinessChanged();
        });
        watcher->setFuture(QtConcurrent::run([path]() { return RomInspector::inspectN64File(path); }));
    }
    return readiness;
}

bool GameLaunchManager::launchProject(const QString &projectId)
{
    return launchGame(projectId, QString());
}

bool GameLaunchManager::launchGame(const QString &projectId, const QString &gamePath)
{
    qCInfo(lcGameLaunch).noquote() << "Play" << projectId << "· game file"
        << (gamePath.isEmpty() ? QStringLiteral("none") : RecompLogging::redactHome(gamePath));
    if (!m_launcherDownloadManager || projectId.trimmed().isEmpty())
        return false;

    if (isRunning(projectId))
        return false;

    if (QProcess *previous = m_processes.take(projectId))
        previous->deleteLater();

    QString executablePath;
    QString workingDirectory = installDirectory(projectId, &executablePath);
    const bool throughWine = runsThroughWine(projectId);
    const bool nativeWindows = !throughWine && isWindowsLauncher(projectId);
    if (throughWine) {
        if (m_wine)
            m_wine->refresh();
        const QVariantMap readiness = wineReadiness(gamePath);
        const QString state = readiness.value(QStringLiteral("state")).toString();
        if (state == QStringLiteral("wine_off") || state == QStringLiteral("wine_missing")) {
            qCWarning(lcGameLaunch) << "Windows launcher not started:" << projectId << state;
            emit launchFailed(projectId, readiness.value(QStringLiteral("title")).toString() + QStringLiteral(". ")
                                             + readiness.value(QStringLiteral("detail")).toString());
            return false;
        }
    }

    const QFileInfo executableInfo(executablePath);
    if (executablePath.isEmpty() || !executableInfo.isFile()
        || executableInfo.isSymLink() || (!throughWine && !executableInfo.isExecutable())) {
        qCWarning(lcGameLaunch) << "Launcher executable validation failed for:" << projectId;
        emit launchFailed(projectId,
                          QStringLiteral("The installed launcher is missing or is not executable."));
        return false;
    }

    LaunchPreparer::Plan plan;
    // Windows launchers get their game file like on Linux, on Windows and
    // through Wine (N64Recomp and HarbourMasters ones, in Wine's C: drive or
    // their own folder; file arguments and selected files as Z:\ paths);
    // other Windows launchers through Wine pick it themselves.
    LaunchPreparer::Options wineOptions;
    const bool prepareThroughWine = throughWine && wineCanPrepare(projectId, &wineOptions);
    if (m_projectCatalog && (!throughWine || prepareThroughWine)) {
        const QVariantMap project = m_projectCatalog->project(projectId);
        if (!project.isEmpty())
            plan = m_preparer.prepare(project, gamePath, launcherFolder(projectId),
                                      throughWine ? wineOptions : preparationOptions(projectId));
        if (plan.defaultFullscreenApplied && m_settingsManager)
            m_settingsManager->markDefaultFullscreenApplied(projectId);
    }
    if (!plan.steps.isEmpty()) {
        QStringList titles;
        for (const QVariant &step : std::as_const(plan.steps))
            titles << step.toMap().value(QStringLiteral("title")).toString();
        qCInfo(lcGameLaunch).noquote() << projectId << "prepared:" << titles.join(QStringLiteral("; "));
    }
    emit launchPrepared(projectId, plan.steps);

    auto *process = new QProcess(this);
    process->setWorkingDirectory(workingDirectory);
    {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        // Set by the AppImage runtime for RecompStation itself; a launcher
        // that is (or checks for) an AppImage would otherwise think it is
        // running from RecompStation's.
        for (const char *name : {"APPDIR", "APPIMAGE", "ARGV0", "OWD", "APPIMAGE_UUID"})
            environment.remove(QString::fromLatin1(name));
        for (auto it = plan.environment.cbegin(); it != plan.environment.cend(); ++it)
            environment.insert(it.key(), it.value());
        process->setProcessEnvironment(environment);
    }

    // Startup scripts often rely on being invoked from their installation
    // directory (for example, "./run.sh") so their relative asset paths work.
    if (throughWine) {
        // wine <program>.exe, from the program's own folder, with the
        // player's environment (and WINEPREFIX) unchanged.
        process->setWorkingDirectory(executableInfo.absolutePath());
        process->setProgram(m_wine->program());
        plan.arguments.prepend(executableInfo.absoluteFilePath());
        qCInfo(lcGameLaunch).noquote() << projectId << "starts through Wine" << m_wine->version();
    } else if (nativeWindows) {
        // <program>.exe from its own folder, where it looks for its files.
        process->setWorkingDirectory(executableInfo.absolutePath());
        process->setProgram(executableInfo.absoluteFilePath());
    } else if (executableInfo.suffix().compare(QStringLiteral("sh"), Qt::CaseInsensitive) == 0) {
        const QString relativePath =
            QDir(workingDirectory).relativeFilePath(executableInfo.absoluteFilePath());
        if (relativePath == QStringLiteral("..")
            || relativePath.startsWith(QStringLiteral("../"))) {
            qCWarning(lcGameLaunch) << "Launcher script is outside its installation directory:"
                                    << projectId;
            emit launchFailed(projectId,
                              QStringLiteral("The launcher script is outside its installation directory."));
            process->deleteLater();
            return false;
        }
        process->setProgram(QStringLiteral("./") + relativePath);
    } else {
        // A program from its own folder, like on Windows: an archive often
        // keeps it in a subfolder, beside the files it looks for.
        process->setWorkingDirectory(executableInfo.absolutePath());
        process->setProgram(executableInfo.absoluteFilePath());
    }
    process->setArguments(plan.arguments);
    process->setProcessChannelMode(QProcess::MergedChannels);
    m_processes.insert(projectId, process);

    auto output = std::make_shared<LauncherOutput>();
    output->open(projectId, executableInfo.absoluteFilePath());
    connect(process, &QProcess::readyReadStandardOutput, this, [process, output]() {
        output->append(process->readAllStandardOutput());
    });

    connect(process, &QProcess::started, this, [this, projectId, process]() {
        qCInfo(lcGameLaunch).noquote() << projectId << "started, process" << process->processId();
        emit launchStarted(projectId, process->processId());
    });

    connect(process, &QProcess::errorOccurred, this,
            [this, projectId, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;

        const QString message = process->errorString();
        qCCritical(lcGameLaunch) << "Could not start" << projectId << ":" << message;
        m_processes.remove(projectId);
        emit launchFailed(projectId, message);
        process->deleteLater();
    });

    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this, projectId, process, output](int exitCode, QProcess::ExitStatus exitStatus) {
        if (m_processes.value(projectId) == process)
            m_processes.remove(projectId);
        output->append(process->readAllStandardOutput());
        output->finish();
        const qint64 seconds = output->timer.elapsed() / 1000;
        const bool crashed = exitStatus == QProcess::CrashExit;
        if (crashed || exitCode != 0) {
            // The launcher's last words are usually the explanation.
            qCWarning(lcGameLaunch).noquote()
                << projectId << (crashed ? QStringLiteral("crashed") : QStringLiteral("exited with code %1").arg(exitCode))
                << QStringLiteral("after %1 s; its last %2 lines of output (full output in %3):")
                       .arg(seconds).arg(output->tail.size())
                       .arg(RecompLogging::redactHome(output->file.fileName()));
            QStringList excerpt;
            for (const QString &line : std::as_const(output->tail))
                excerpt << QStringLiteral("    | ") + line;
            RecompLogging::writeBlock(excerpt);
        } else {
            qCInfo(lcGameLaunch).noquote() << projectId << "closed normally after"
                                           << seconds << "s";
        }
        emit processFinished(projectId, exitCode,
                             exitStatus == QProcess::CrashExit);
        process->deleteLater();
    });

    process->start();
    return true;
}

bool GameLaunchManager::isRunning(const QString &projectId) const
{
    const QProcess *process = m_processes.value(projectId, nullptr);
    return process && process->state() != QProcess::NotRunning;
}
