#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>

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

private:
    struct Download;

    PlatformManager *m_platformManager = nullptr;
    QNetworkAccessManager m_network;
    QHash<int, Download *> m_downloads;
    QSet<QString> m_activeTargets;
    int m_nextRequestId = 1;

    void receiveDownloadData(int requestId);
    void finishDownload(int requestId);
    void failDownload(int requestId, const QString &message);

    static bool isValidProjectId(const QString &projectId);
    static bool isValidAssetName(const QString &assetName);
    static bool isValidHttpsUrl(const QString &url);
};
