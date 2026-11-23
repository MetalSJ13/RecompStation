#pragma once

#include "LaunchBoxXmlParser.h"

#include <QList>
#include <QString>
#include <QSqlDatabase>
#include <QStringList>
#include <atomic>
#include <functional>

class LaunchBoxCatalogDatabase
{
public:
    LaunchBoxCatalogDatabase();
    ~LaunchBoxCatalogDatabase();

    LaunchBoxCatalogDatabase(const LaunchBoxCatalogDatabase &) = delete;
    LaunchBoxCatalogDatabase &operator=(const LaunchBoxCatalogDatabase &) = delete;

    bool open(const QString &databasePath);
    void close();
    bool isOpen() const;
    QString lastError() const;

    // Streams each XML record into a single SQLite transaction. If parsing or
    // indexing fails, rollback leaves the previous catalog intact.
    bool rebuildFromXmlFiles(const QString &platformsXmlPath,
                             const QString &filesXmlPath,
                             const QString &metadataXmlPath,
                             const std::atomic_bool *cancelled = nullptr,
                             const std::function<void(const QString &, int)> &progress = {});

    // When set, rebuildFromXmlFiles() keeps only games, file associations
    // and images for these LaunchBox platform names (for example
    // "Nintendo 64"). The platform list itself is always kept. Empty keeps all.
    void setPlatformFilter(const QStringList &launchBoxPlatformNames);

    QList<LaunchBoxFileAssociation> findFileAssociations(
        const QString &internalPlatform, const QString &localFileName) const;
    QList<LaunchBoxGameRecord> findGamesByTitle(
        const QString &internalPlatform, const QString &title) const;
    QList<LaunchBoxGameRecord> findGamesByRevisionInsensitiveTitle(
        const QString &internalPlatform, const QString &title) const;
    bool gameByDatabaseId(int databaseId, LaunchBoxGameRecord *game) const;
    QList<LaunchBoxGameImageRecord> imagesByDatabaseId(int databaseId) const;

    int platformCount() const;
    int fileAssociationCount() const;
    int gameCount() const;
    int gameImageCount() const;
    QString indexedAt() const;

private:
    QSqlDatabase m_database;
    QString m_connectionName;
    QStringList m_platformFilter;
    QString m_lastError;

    bool createSchema();
    bool setError(const QString &message);
};
