#pragma once

#include <QFutureWatcher>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>

#include "ArchiveExtractor.h"

class PlatformManager;
class QNetworkReply;

class LauncherDownloadManager : public QObject
{
    Q_OBJECT

public:
    explicit LauncherDownloadManager(
        PlatformManager *platformManager,
        QObject *parent = nullptr
    );
    ~LauncherDownloadManager() override;

    // expectedSha256: the hex SHA-256 the release publishes for this asset
    // (GitHub's asset "digest"). When given, a download that doesn't match is
    // refused; without one the download is kept but counted as unverified.
    Q_INVOKABLE int downloadAsset(
        const QString &projectId,
        const QString &assetName,
        const QString &downloadUrl,
        qint64 expectedSize = -1,
        const QString &expectedSha256 = QString()
    );

    Q_INVOKABLE void cancelDownload(int requestId);

    // releaseTag is recorded so Game Details can show the installed version.
    // With replaceExisting, an installed launcher is updated: the new version
    // is unpacked beside it, the two folders are swapped, the player's own
    // files move across, and any failure puts the old version back.
    Q_INVOKABLE int installDownloadedAsset(
        const QString &projectId,
        const QString &assetName,
        const QString &packageFormat,
        const QString &expectedExecutable,
        const QString &releaseTag = QString(),
        bool replaceExisting = false,
        // "linux-x86_64" or "windows-x86_64": recorded with the install. A
        // Windows build may leave expectedExecutable empty: the download's
        // one main .exe is used.
        const QString &build = QString()
    );

    // SHA-256 of a file as lowercase hex, or an empty string if unreadable.
    static QString sha256OfFile(const QString &path);
    // Finishes or undoes an update that was interrupted mid-swap (run at
    // startup). Returns how many were recovered.
    int recoverInterruptedUpdates();

    Q_INVOKABLE QVariantList installedLaunchers() const;
    // Removes a managed launcher. Files the launcher or player created in its
    // folder (saves, settings, portable mode) are first moved to
    // keptFilesDirectory(), and put back when the launcher is installed again.
    Q_INVOKABLE bool uninstallLauncher(const QString &projectId);
    // Kept apart per build: a Windows launcher's files never end up in the
    // Linux launcher's folder, or the reverse.
    Q_INVOKABLE QString keptFilesDirectory(const QString &projectId, const QString &build = QString()) const;
    // The build recorded in an installed launcher's folder ("" for Linux
    // installs from before builds were recorded).
    static QString installedBuild(const QString &installDirectory);
    // At startup, before any download: removes what interrupted downloads
    // and installs left in the staging area (partial files, extraction
    // folders, archives that were never installed). Nothing there is reused
    // after a restart. Returns how many entries were removed, or -1 when the
    // staging area isn't where it should be (then nothing is touched).
    int cleanUpStaging();

signals:
    void downloadProgress(
        int requestId,
        qint64 bytesReceived,
        qint64 bytesTotal
    );
    void downloadCompleted(
        int requestId,
        const QString &projectId,
        const QString &assetName,
        const QString &filePath
    );
    void downloadFailed(int requestId, const QString &message);
    void installationCompleted(
        int requestId,
        const QString &projectId,
        const QString &executablePath
    );
    void installationFailed(int requestId, const QString &message);

private:
    // Returns how many files were moved, or -1 when one couldn't be.
    int setAsideUserFiles(const QString &projectId, const QString &installDirectory);
    int restoreKeptFiles(const QString &projectId, const QString &installDirectory);
    // Files the player or launcher created in an install (not in its package).
    static QStringList playerFiles(const QString &installDirectory);
    // Swaps a freshly unpacked version in for the installed one.
    bool replaceInstallation(const QString &projectId, const QString &newDirectory,
                             const QString &destination, const QString &executableRelative,
                             QString *error);
    QString launcherRoot() const;
    // What downloads verified, by staged file path, until it is installed.
    struct Checksum { QString sha256; bool verified = false; };
    QHash<QString, Checksum> m_checksums;

    struct Download;

    PlatformManager *m_platformManager = nullptr;
    QNetworkAccessManager m_network;
    QHash<int, Download *> m_downloads;
    QHash<int, QFutureWatcher<ArchiveExtractor::Result> *> m_installations;
    QSet<QString> m_activeTargets;
    QSet<QString> m_activeInstallProjects;
    int m_nextRequestId = 1;

    void receiveDownloadData(int requestId);
    void finishDownload(int requestId);
    void failDownload(int requestId, const QString &message);

    static bool isValidProjectId(const QString &projectId);
    static bool isValidAssetName(const QString &assetName);
    static bool isValidHttpsUrl(const QString &url);
    static bool isSafeRelativePath(const QString &path);
};
