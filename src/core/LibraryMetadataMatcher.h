#pragma once

#include <QObject>
#include <QQueue>
#include <QVariantList>
#include <QVariantMap>

class ArtworkManager;
class DatabaseManager;
class MetadataManager;
class LaunchBoxCatalogManager;
class SettingsManager;

class LibraryMetadataMatcher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(int completedCount READ completedCount NOTIFY progressChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit LibraryMetadataMatcher(DatabaseManager *database,
                                    MetadataManager *metadata,
                                    ArtworkManager *artwork,
                                    LaunchBoxCatalogManager *launchBoxCatalog,
                                    SettingsManager *settings,
                                    QObject *parent = nullptr);

    bool isBusy() const;
    int completedCount() const;
    int totalCount() const;
    QString status() const;

    Q_INVOKABLE void scrapeLibrary(bool includePreviouslyChecked = false,
                                   bool forceArtworkDownload = false);
    Q_INVOKABLE void scrapeGame(int gameId);

    static int exactMatchIndex(const QString &localTitle,
                               const QVariantList &results);
    static int bestMatchIndex(const QString &localTitle, int platformId,
                              const QVariantList &results);

signals:
    void busyChanged();
    void progressChanged();
    void statusChanged();
    void scrapeFinished(int matched, int unresolved);

private:
    enum class LaunchBoxOutcome { Matched, NoMatch, Ambiguous, Unavailable, Failed };
    // Looks the current game up in the LaunchBox catalog and saves a unique
    // match. Does not record the attempt or advance the queue.
    LaunchBoxOutcome attemptLaunchBoxMatch(QString *message);
    // After TheGamesDB could not match (or failed): try LaunchBox unless it
    // was already asked, record the result and move on. Always returns true.
    bool tryLaunchBoxFallback(const QString &reason,
                              const QString &unmatchedStatus = QStringLiteral("no_match"));
    bool launchBoxAvailable() const;
    void markCurrent(const QString &status);
    void runPendingRescan();
    void processNext();
    void finishCurrent(bool matched, const QString &reason = {});
    void setBusy(bool busy);
    void setStatus(const QString &status);

    DatabaseManager *m_database = nullptr;
    MetadataManager *m_metadata = nullptr;
    ArtworkManager *m_artwork = nullptr;
    LaunchBoxCatalogManager *m_launchBoxCatalog = nullptr;
    SettingsManager *m_settings = nullptr;
    QQueue<QVariantMap> m_queue;
    QVariantMap m_currentGame;
    int m_completedCount = 0;
    int m_totalCount = 0;
    int m_matchedCount = 0;
    int m_unresolvedCount = 0;
    bool m_busy = false;
    bool m_forceArtworkDownload = false;
    bool m_theGamesDbUnavailable = false;
    bool m_singleGameRun = false;
    QString m_theGamesDbFailure;
    bool m_rescanPending = false;
    bool m_rescanIncludesChecked = false;
    QString m_status;
};
