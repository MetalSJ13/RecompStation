#include "GameLaunchManager.h"

#include "LauncherDownloadManager.h"
#include "Logging.h"
#include "SettingsManager.h"
#include "projects/ProjectCatalog.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QVariantMap>

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
    qCInfo(lcGameLaunch) << "Launch requested for project:" << projectId;
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
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    m_processes.insert(projectId, process);

    connect(process, &QProcess::started, this, [this, projectId, process]() {
        qCInfo(lcGameLaunch) << "Process started for" << projectId
                             << "PID:" << process->processId();
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
            [this, projectId, process](int exitCode, QProcess::ExitStatus exitStatus) {
        if (m_processes.value(projectId) == process)
            m_processes.remove(projectId);
        qCInfo(lcGameLaunch) << "Process finished for" << projectId
                             << "exit code:" << exitCode
                             << "crashed:" << (exitStatus == QProcess::CrashExit);
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
