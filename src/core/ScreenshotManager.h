#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QStringList>
#include <QVariantMap>

class DatabaseManager;

class ScreenshotManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit ScreenshotManager(DatabaseManager *database,
                               const QString &artworkDirectory,
                               const QString &apiKey,
                               QObject *parent = nullptr);
    bool isBusy() const;
    QString status() const;
    Q_INVOKABLE void refreshLibraryScreenshots();
    void setProviderEnabled(bool enabled);

signals:
    void busyChanged();
    void statusChanged();
    void refreshFinished(int downloaded, int skipped, int withoutScreenshots, int failed);

private:
    void processNextGame();
    void downloadNextImage();
    void setBusy(bool busy);
    void setStatus(const QString &status);

    DatabaseManager *m_database = nullptr;
    QString m_artworkDirectory;
    QString m_apiKey;
    QNetworkAccessManager m_network;
    QQueue<QVariantMap> m_queue;
    QStringList m_pendingUrls;
    int m_currentGameId = 0;
    int m_currentProviderGameId = 0;
    int m_downloaded = 0;
    int m_skipped = 0;
    int m_withoutScreenshots = 0;
    int m_failed = 0;
    bool m_busy = false;
    bool m_providerEnabled = true;
    QString m_status;
};
