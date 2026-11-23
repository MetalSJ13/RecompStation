#include "GameLaunchManager.h"

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
#include <QProcessEnvironment>
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

void GameLaunchManager::setPreparationRoots(const QString &homeDirectory, const QString &dataHome)
{
    m_homeDirectory = homeDirectory;
    m_dataHome = dataHome;
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
    return m_preparer.readiness(project, gamePath, installDirectory(projectId),
                                preparationOptions(projectId));
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
    const QString workingDirectory = installDirectory(projectId, &executablePath);

    const QFileInfo executableInfo(executablePath);
    if (executablePath.isEmpty() || !executableInfo.isFile()
        || executableInfo.isSymLink() || !executableInfo.isExecutable()) {
        qCWarning(lcGameLaunch) << "Launcher executable validation failed for:" << projectId;
        emit launchFailed(projectId,
                          QStringLiteral("The installed launcher is missing or is not executable."));
        return false;
    }

    LaunchPreparer::Plan plan;
    if (m_projectCatalog) {
        const QVariantMap project = m_projectCatalog->project(projectId);
        if (!project.isEmpty())
            plan = m_preparer.prepare(project, gamePath, workingDirectory, preparationOptions(projectId));
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
    if (!plan.environment.isEmpty()) {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        for (auto it = plan.environment.cbegin(); it != plan.environment.cend(); ++it)
            environment.insert(it.key(), it.value());
        process->setProcessEnvironment(environment);
    }

    // Startup scripts often rely on being invoked from their installation
    // directory (for example, "./run.sh") so their relative asset paths work.
    if (executableInfo.suffix().compare(QStringLiteral("sh"), Qt::CaseInsensitive) == 0) {
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
        process->setProgram(executableInfo.absoluteFilePath());
    }
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
