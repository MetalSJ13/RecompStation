#pragma once

#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QElapsedTimer>
#include <QSaveFile>

#include <atomic>
#include <memory>

class QNetworkReply;

struct LaunchBoxCatalogBuildResult
{
    bool success = false;
    int platformCount = 0;
    int fileCount = 0;
    int gameCount = 0;
    QString error;
};

class LaunchBoxCatalogManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString catalogPath READ catalogPath CONSTANT)
    Q_PROPERTY(bool catalogAvailable READ catalogAvailable NOTIFY catalogAvailableChanged)
    Q_PROPERTY(qint64 catalogSizeBytes READ catalogSizeBytes NOTIFY catalogAvailableChanged)
    // Library systems the installed catalog has no games for, because it was
    // downloaded before RecompStation supported them (internal names such as
    // "PS2"). Update catalog adds them.
    Q_PROPERTY(QStringList missingSystems READ missingSystems NOTIFY missingSystemsChanged)

public:
    explicit LaunchBoxCatalogManager(const QString &dataDirectory,
                                     QObject *parent = nullptr);
    ~LaunchBoxCatalogManager() override;

    bool busy() const;
    QString catalogPath() const;
    bool catalogAvailable() const;

    qint64 catalogSizeBytes() const;
    // RecompStation platform names (N64, PS1, ...) to keep when indexing.
    // Other LaunchBox platforms are skipped. Empty indexes everything.
    void setIndexedPlatforms(const QStringList &internalPlatforms);
    QStringList missingSystems() const;
    // Re-reads which systems the catalog covers (after setIndexedPlatforms
    // and after an update).
    void refreshMissingSystems();

    Q_INVOKABLE int updateCatalog();
    Q_INVOKABLE void cancelUpdate();

signals:
    void busyChanged();
    void catalogAvailableChanged();
    void downloadProgress(qint64 received, qint64 total);
    void updateProgress(const QString &stage, int percent);
    void updateCompleted(int platformCount, int fileCount, int gameCount);
    void updateFailed(const QString &message);
    void updateCancelled();
    void missingSystemsChanged();

private:
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QSaveFile m_downloadFile;
    QElapsedTimer m_updateTimer; // for the log: how long each stage took
    QFutureWatcher<LaunchBoxCatalogBuildResult> m_buildWatcher;
    std::shared_ptr<std::atomic_bool> m_cancelFlag;
    QString m_dataDirectory;
    QString m_catalogDirectory;
    QString m_catalogPath;
    QString m_archivePath;
    QString m_extractPath;
    QString m_temporaryDatabasePath;
    QString m_backupDatabasePath;
    QStringList m_launchBoxPlatformFilter;
    QStringList m_internalPlatforms;
    QStringList m_missingSystems;
    bool m_busy = false;
    bool m_cancelRequested = false;
    int m_requestId = 0;

    void setBusy(bool busy);
    void receiveDownloadData();
    void finishDownload();
    void startBackgroundBuild();
    void finishBackgroundBuild();
    void failUpdate(const QString &message);
    void cleanupStaging();
    bool validateAndInstallCatalog(QString *errorMessage);
};
