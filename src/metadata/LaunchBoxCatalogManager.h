#pragma once

#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
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

public:
    explicit LaunchBoxCatalogManager(const QString &dataDirectory,
                                     QObject *parent = nullptr);
    ~LaunchBoxCatalogManager() override;

    bool busy() const;
    QString catalogPath() const;
    bool catalogAvailable() const;

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

private:
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QSaveFile m_downloadFile;
    QFutureWatcher<LaunchBoxCatalogBuildResult> m_buildWatcher;
    std::shared_ptr<std::atomic_bool> m_cancelFlag;
    QString m_dataDirectory;
    QString m_catalogDirectory;
    QString m_catalogPath;
    QString m_archivePath;
    QString m_extractPath;
    QString m_temporaryDatabasePath;
    QString m_backupDatabasePath;
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
