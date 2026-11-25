#pragma once

#include "github/GitHubManager.h"

#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QSaveFile>
#include <QString>

class QNetworkReply;

// Checks GitHub for a newer RecompStation release and, when the player
// agrees, updates this copy:
//   AppImage      the new AppImage is downloaded into the same folder,
//                 checked, and swapped in under the same name; the old one
//                 is kept (hidden) until the new version has started.
//   Portable zip  the new zip is checked and unpacked beside the folder,
//                 and the two folders are swapped when RecompStation
//                 restarts or quits; the old folder is kept until the new
//                 version has started.
//   Source build  never updated; the player is pointed to the website.
// Every download is checked against the SHA-256 GitHub publishes for it.
class AppUpdater : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString installKind READ installKind CONSTANT)
    Q_PROPERTY(bool canInstall READ canInstall CONSTANT)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY releaseChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY releaseChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY releaseChanged)
    Q_PROPERTY(QString releaseDate READ releaseDate NOTIFY releaseChanged)
    Q_PROPERTY(qint64 downloadSize READ downloadSize NOTIFY releaseChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY releaseChanged)
    Q_PROPERTY(qint64 bytesReceived READ bytesReceived NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesTotal READ bytesTotal NOTIFY progressChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(QString lastChecked READ lastChecked NOTIFY releaseChanged)
    Q_PROPERTY(bool checkAutomatically READ checkAutomatically WRITE setCheckAutomatically NOTIFY settingsChanged)
    Q_PROPERTY(QString skippedVersion READ skippedVersion NOTIFY settingsChanged)
    Q_PROPERTY(bool promptPending READ promptPending NOTIFY promptPendingChanged)

public:
    struct Install
    {
        QString kind;   // "appimage", "zip" or "source"
        QString path;   // the AppImage file, or the portable folder
    };

    explicit AppUpdater(const QString &workDirectory, QObject *parent = nullptr);

    QString currentVersion() const;
    QString installKind() const { return m_install.kind; }
    bool canInstall() const { return m_install.kind != QStringLiteral("source"); }
    QString state() const { return m_state; }
    QString latestVersion() const { return m_latestVersion; }
    QString releaseNotes() const { return m_releaseNotes; }
    QString releaseUrl() const { return m_releaseUrl; }
    QString releaseDate() const { return m_releaseDate; }
    qint64 downloadSize() const { return m_asset.size; }
    bool updateAvailable() const;
    qint64 bytesReceived() const { return m_bytesReceived; }
    qint64 bytesTotal() const { return m_bytesTotal; }
    QString errorMessage() const { return m_errorMessage; }
    QString lastChecked() const;
    bool checkAutomatically() const;
    void setCheckAutomatically(bool enabled);
    QString skippedVersion() const;
    bool promptPending() const { return m_promptPending; }

    // Startup: shows a remembered newer version again ("Remind me later"),
    // and checks GitHub when automatic checks are on and the last check is
    // more than a day old.
    Q_INVOKABLE void checkAtStartup();
    Q_INVOKABLE void checkNow();
    Q_INVOKABLE void startUpdate();
    Q_INVOKABLE void cancelUpdate();
    Q_INVOKABLE void remindLater();
    Q_INVOKABLE void skipLatestVersion();
    Q_INVOKABLE void undoSkip();
    Q_INVOKABLE void clearSkippedVersion();
    // Restart into the new version (emits restartRequested).
    Q_INVOKABLE void restartNow();

    // Puts a staged portable-zip update in place; call just before
    // restarting and when quitting. Returns false when nothing was staged
    // or the swap failed (the current version then stays).
    bool applyStagedUpdate();
    // At startup: removes what a finished update left (the hidden old
    // AppImage or folder). Returns true when a staged portable update was
    // found and put in place, so the caller should restart into it.
    bool finishAtStartup();

    // Building blocks, public for tests.
    static Install detectInstall(const QString &appImageEnvironment,
                                 const QString &applicationFilePath);
    static int compareVersions(const QString &left, const QString &right);
    static GitHubReleaseAsset selectAsset(const QList<GitHubReleaseAsset> &assets,
                                          const QString &installKind);
    static QString changesSection(const QString &releaseBody);
    static bool replaceAppImage(const QString &currentPath, const QString &newFile,
                                QString *error);
    static bool stageZipUpdate(const QString &zipPath, const QString &portableRoot,
                               const QString &version, QString *error);
    static bool applyStagedZip(const QString &portableRoot, QString *error);
    static QString previousPath(const QString &path);
    static QString stagedPath(const QString &portableRoot);
    static QString stagedVersion(const QString &portableRoot);

    void setInstallForTesting(const Install &install) { m_install = install; }
    void setReleaseForTesting(const GitHubRelease &release) { applyRelease(release, false); updatePrompt(); }

signals:
    void stateChanged();
    void releaseChanged();
    void progressChanged();
    void settingsChanged();
    void promptPendingChanged();
    void restartRequested();
    // A short note for the player ("Skipped 0.2.0"), with Undo where useful.
    void notice(const QString &message, bool canUndo);

private:
    void setState(const QString &state, const QString &error = QString());
    void setPromptPending(bool pending);
    void applyRelease(const GitHubRelease &release, bool fromNetwork);
    void requestRelease();
    void finishDownload();
    void failUpdate(const QString &message);
    void rememberRelease(const GitHubRelease &release) const;
    bool restoreRememberedRelease();
    void updatePrompt();

    Install m_install;
    QString m_workDirectory;
    QString m_state = QStringLiteral("idle");
    QString m_errorMessage;
    QString m_latestVersion;
    QString m_releaseNotes;
    QString m_releaseUrl;
    QString m_releaseDate;
    GitHubReleaseAsset m_asset;
    qint64 m_bytesReceived = 0;
    qint64 m_bytesTotal = 0;
    bool m_promptPending = false;
    bool m_promptAnswered = false;
    bool m_userInitiated = false;
    QString m_undoSkipValue;
    int m_releaseRequest = -1;
    GitHubManager m_github;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QSaveFile *m_file = nullptr;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QString m_downloadPath;
};
