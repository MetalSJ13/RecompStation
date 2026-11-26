#pragma once

#include "LaunchPreparer.h"

#include <QHash>
#include <QSet>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class LauncherDownloadManager;
class ProjectCatalog;
class SettingsManager;
class WineSupport;
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
    // Windows launchers run through this Wine (Linux only).
    void setWineSupport(WineSupport *wine);
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
    // A game file finished being checked in the background: ask
    // launchReadiness() again for an up-to-date answer.
    void launchReadinessChanged();

private:
    QString installDirectory(const QString &projectId, QString *executablePath = nullptr) const;
    LaunchPreparer::Options preparationOptions(const QString &projectId) const;
    // Whether the installed launcher is a Windows build.
    bool isWindowsLauncher(const QString &projectId) const;
    // A Windows launcher on Linux, started through Wine (not on Windows).
    bool runsThroughWine(const QString &projectId) const;
    QString launcherFolder(const QString &projectId) const;
    // Through Wine, N64Recomp and HarbourMasters launchers can be given the
    // game file as on Windows: their folders inside Wine's C: drive.
    bool wineCanPrepare(const QString &projectId, LaunchPreparer::Options *options) const;
    // The line under Play for a Windows launcher, and why it can't start.
    QVariantMap wineReadiness(const QString &gamePath) const;

    LauncherDownloadManager *m_launcherDownloadManager = nullptr;
    ProjectCatalog *m_projectCatalog = nullptr;
    SettingsManager *m_settingsManager = nullptr;
    WineSupport *m_wine = nullptr;
    QString m_homeDirectory;
    QString m_dataHome;
    LaunchPreparer m_preparer;
    QHash<QString, QProcess *> m_processes;
    QSet<QString> m_inspecting;  // game files being read in the background
};
