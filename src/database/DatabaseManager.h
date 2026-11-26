#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantList>

class DatabaseManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    bool initialize(const QString &path);
    QString lastError() const;

    bool reconcilePlatformGames(const QString &platform,
                                const QStringList &discoveredPaths);
    // Several changes saved as one transaction: one write to disk instead
    // of one per call (each waits for the disk). registerGame and
    // reconcilePlatformGames called in between join it.
    bool beginBatch();
    bool endBatch();
    Q_INVOKABLE bool registerGame(const QString &title,
                                  const QString &platform,
                                  const QString &gamePath);
    Q_INVOKABLE QVariantList games(const QString &platform = QString()) const;
    Q_INVOKABLE bool associateMetadata(int gameId,
                                       const QString &provider,
                                       int providerGameId,
                                       const QString &metadataTitle,
                                       const QString &overview,
                                       const QString &releaseDate,
                                       int providerPlatformId,
                                       const QString &rating,
                                       const QString &artworkUrl);
    // Inserts metadata only when the local game has no provider association.
    // Existing metadata is never replaced by this fallback-safe operation.
    bool associateMetadataIfMissing(int gameId,
                                    const QString &provider,
                                    int providerGameId,
                                    const QString &metadataTitle,
                                    const QString &overview,
                                    const QString &releaseDate,
                                    int providerPlatformId,
                                    const QString &rating,
                                    const QString &artworkUrl);
    Q_INVOKABLE bool setArtworkPath(int gameId, const QString &artworkPath);
    Q_INVOKABLE QVariantList gamesWithMetadata() const;
    Q_INVOKABLE bool addGameScreenshot(int gameId, const QString &localPath);
    // Records that TheGamesDB was asked for this game's screenshots.
    bool markScreenshotsChecked(int gameId, int providerGameId);
    Q_INVOKABLE bool clearGameScreenshots(int gameId);
    Q_INVOKABLE QVariantList gameScreenshots(int gameId) const;
    Q_INVOKABLE QVariantList gamesNeedingMetadata(bool includePreviouslyChecked = false) const;
    Q_INVOKABLE bool markMetadataLookupAttempt(int gameId, const QString &status);

signals:
    void errorChanged();

private:
    QSqlDatabase m_database;
    QString m_lastError;
    bool m_batchActive = false;
    bool createTables();
    bool associateMetadataInternal(int gameId,
                                   const QString &provider,
                                   int providerGameId,
                                   const QString &metadataTitle,
                                   const QString &overview,
                                   const QString &releaseDate,
                                   int providerPlatformId,
                                   const QString &rating,
                                   const QString &artworkUrl,
                                   bool onlyIfMissing);
};
