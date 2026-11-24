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

    Q_INVOKABLE int downloadAsset(
        const QString &projectId,
        const QString &assetName,
        const QString &downloadUrl,
        qint64 expectedSize = -1
    );

    Q_INVOKABLE void cancelDownload(int requestId);

    Q_INVOKABLE int installDownloadedAsset(
        const QString &projectId,
        const QString &assetName,
        const QString &packageFormat,
        const QString &expectedExecutable
    );

    Q_INVOKABLE QVariantList installedLaunchers() const;
    // Removes a managed launcher. Files the launcher or player created in its
    // folder (saves, settings, portable mode) are first moved to
    // keptFilesDirectory(), and put back when the launcher is installed again.
    Q_INVOKABLE bool uninstallLauncher(const QString &projectId);
    Q_INVOKABLE QString keptFilesDirectory(const QString &projectId) const;
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
