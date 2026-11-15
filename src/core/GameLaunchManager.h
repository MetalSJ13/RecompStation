#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class LauncherDownloadManager;
class QProcess;

class GameLaunchManager : public QObject
{
    Q_OBJECT

public:
    explicit GameLaunchManager(
        LauncherDownloadManager *launcherDownloadManager,
        QObject *parent = nullptr
    );
    ~GameLaunchManager() override;

    Q_INVOKABLE bool launchProject(const QString &projectId);
    Q_INVOKABLE bool isRunning(const QString &projectId) const;

signals:
    void launchStarted(const QString &projectId, qint64 processId);
    void launchFailed(const QString &projectId, const QString &message);
    void processFinished(const QString &projectId, int exitCode, bool crashed);

private:
    LauncherDownloadManager *m_launcherDownloadManager = nullptr;
    QHash<QString, QProcess *> m_processes;
};
