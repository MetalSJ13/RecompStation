#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
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
    Q_INVOKABLE bool setArtworkPath(int gameId, const QString &artworkPath);
    Q_INVOKABLE QVariantList gamesNeedingMetadata(bool includePreviouslyChecked = false) const;
    Q_INVOKABLE bool markMetadataLookupAttempt(int gameId, const QString &status);

signals:
    void errorChanged();

private:
    QSqlDatabase m_database;
    QString m_lastError;
    bool createTables();
};
