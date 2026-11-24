#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QQueue>
#include <QStringList>
#include <QVariantMap>

class DatabaseManager;
class TheGamesDbAllowance;

class ScreenshotManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    // TheGamesDB's Games/Images takes a comma-separated list of game ids,
    // so one request covers this many games (plus any extra result pages).
    static constexpr int GamesPerRequest = 20;

    explicit ScreenshotManager(DatabaseManager *database,
                               const QString &artworkDirectory,
                               const QString &apiKey,
                               QObject *parent = nullptr);
    bool isBusy() const;
    QString status() const;
    // Downloads screenshots for matched games that have none. Games
    // TheGamesDB was asked about in the last 30 days are skipped unless
    // recheckAll is true (Refresh all metadata).
    Q_INVOKABLE void refreshLibraryScreenshots(bool recheckAll = false);
    void setProviderEnabled(bool enabled);
    void setApiKey(const QString &apiKey);
    void setAllowance(TheGamesDbAllowance *allowance);

    // Reads a Games/Images response: screenshot URLs per TheGamesDB game id,
    // and the page number of the next result page (0 when there is none).
    static bool parseImagesResponse(const QByteArray &response,
                                    QHash<int, QStringList> *screenshots,
                                    int *nextPage,
                                    QString *errorMessage = nullptr);

signals:
    void busyChanged();
    void statusChanged();
    void refreshFinished(int downloaded, int skipped, int withoutScreenshots, int failed);

private:
    void requestNextBatch();
    void requestImagesPage(int page);
    void finishBatch();
    void processNextGame();
    void downloadNextImage();
    void finishRefresh(const QString &note = QString());
    void setBusy(bool busy);
    void setStatus(const QString &status);

    DatabaseManager *m_database = nullptr;
    QString m_artworkDirectory;
    QString m_apiKey;
    QPointer<TheGamesDbAllowance> m_allowance;
    QNetworkAccessManager m_network;
    // Games still to look up, and the batch being looked up now.
    QQueue<QVariantMap> m_queue;
    QList<QVariantMap> m_batch;
    QHash<int, QStringList> m_batchScreenshots;
    // Games of the finished batch whose screenshots are being downloaded.
    QQueue<QVariantMap> m_downloadQueue;
    QStringList m_pendingUrls;
    int m_currentGameId = 0;
    int m_currentProviderGameId = 0;
    int m_currentGameImageCount = 0;
    int m_downloaded = 0;
    int m_skipped = 0;
    int m_withoutScreenshots = 0;
    int m_failed = 0;
    bool m_busy = false;
    bool m_providerEnabled = true;
    QString m_status;
};
