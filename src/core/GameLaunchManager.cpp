#include "GameLaunchManager.h"

#include "LauncherDownloadManager.h"
#include "Logging.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
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

bool GameLaunchManager::launchProject(const QString &projectId)
{
    qCInfo(lcGameLaunch) << "Launch requested for project:" << projectId;
    if (!m_launcherDownloadManager || projectId.trimmed().isEmpty())
        return false;

    if (isRunning(projectId))
        return false;

    if (QProcess *previous = m_processes.take(projectId))
        previous->deleteLater();

    QString executablePath;
    QString workingDirectory;
    for (const QVariant &entry : m_launcherDownloadManager->installedLaunchers()) {
        const QVariantMap installation = entry.toMap();
        if (installation.value(QStringLiteral("projectId")).toString() == projectId) {
            executablePath = installation.value(QStringLiteral("executablePath")).toString();
            workingDirectory = installation.value(QStringLiteral("directory")).toString();
            break;
        }
    }

    const QFileInfo executableInfo(executablePath);
    if (executablePath.isEmpty() || !executableInfo.isFile()
        || executableInfo.isSymLink() || !executableInfo.isExecutable()) {
        qCWarning(lcGameLaunch) << "Launcher executable validation failed for:" << projectId;
        emit launchFailed(projectId,
                          QStringLiteral("The installed launcher is missing or is not executable."));
        return false;
    }

    auto *process = new QProcess(this);
    process->setWorkingDirectory(workingDirectory);

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
