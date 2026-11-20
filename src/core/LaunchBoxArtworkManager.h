#pragma once

#include "../metadata/LaunchBoxXmlParser.h"

#include <QObject>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QTimer>
#include <QVariantMap>

class DatabaseManager;

struct LaunchBoxArtworkRequest
{
    int gameId = 0;
    QString fileName;
    QString type;
    QString destination;
};

class LaunchBoxArtworkManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit LaunchBoxArtworkManager(DatabaseManager *database,
                                     const QString &dataDirectory,
                                     QObject *parent = nullptr);

    bool busy() const;
    QString status() const;
    Q_INVOKABLE void refreshLibraryArtwork();

    static QString imageUrl(const QString &fileName);
    static int regionPreference(const QString &region);
    static QList<LaunchBoxArtworkRequest> selectRequests(
        int libraryGameId, int launchBoxGameId,
        const QList<LaunchBoxGameImageRecord> &images,
        bool includeCover, int maximumScreenshots = 3);

signals:
    void busyChanged();
    void statusChanged();
    void refreshFinished(int downloaded, int skipped, int failed);

private:
    struct PendingRequest {
        int gameId = 0;
        QString destination;
        QString type;
        QString url;
    };

    void processNext();
    void setBusy(bool value);
    void setStatus(const QString &value);

    DatabaseManager *m_database = nullptr;
    QString m_artworkDirectory;
    QString m_catalogPath;
    QNetworkAccessManager m_network;
    QQueue<PendingRequest> m_queue;
    QTimer m_batchPause;
    int m_batchRequests = 0;
    int m_totalRequests = 0;
    int m_downloaded = 0;
    int m_skipped = 0;
    int m_failed = 0;
    bool m_busy = false;
    bool m_refreshPending = false;
    QString m_status;
};
