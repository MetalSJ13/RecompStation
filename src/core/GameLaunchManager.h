#pragma once

#include "LaunchPreparer.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class LauncherDownloadManager;
class ProjectCatalog;
class SettingsManager;
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

    void setProjectCatalog(ProjectCatalog *catalog);
    void setSettingsManager(SettingsManager *settings);
    // Folders used instead of the user's home in tests.
    void setPreparationRoots(const QString &homeDirectory, const QString &dataHome);

    Q_INVOKABLE bool launchProject(const QString &projectId);
    // Prepares the launcher with the player's game file (see LaunchPreparer)
    // and starts it.
    Q_INVOKABLE bool launchGame(const QString &projectId, const QString &gamePath);
    Q_INVOKABLE bool isRunning(const QString &projectId) const;
    // The status line under Play: whether Play will just work.
    Q_INVOKABLE QVariantMap launchReadiness(const QString &projectId, const QString &gamePath);

signals:
    void launchPrepared(const QString &projectId, const QVariantList &steps);
    void launchStarted(const QString &projectId, qint64 processId);
    void launchFailed(const QString &projectId, const QString &message);
    void processFinished(const QString &projectId, int exitCode, bool crashed);

private:
    QString installDirectory(const QString &projectId, QString *executablePath = nullptr) const;
    LaunchPreparer::Options preparationOptions(const QString &projectId) const;

    LauncherDownloadManager *m_launcherDownloadManager = nullptr;
    ProjectCatalog *m_projectCatalog = nullptr;
    SettingsManager *m_settingsManager = nullptr;
    QString m_homeDirectory;
    QString m_dataHome;
    LaunchPreparer m_preparer;
    QHash<QString, QProcess *> m_processes;
};
