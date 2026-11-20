#pragma once

#include "LaunchBoxXmlParser.h"

#include <QList>
#include <QString>
#include <QSqlDatabase>

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
                             const QString &metadataXmlPath);

    QList<LaunchBoxFileAssociation> findFileAssociations(
        const QString &internalPlatform, const QString &localFileName) const;
    QList<LaunchBoxGameRecord> findGamesByTitle(
        const QString &internalPlatform, const QString &title) const;
    bool gameByDatabaseId(int databaseId, LaunchBoxGameRecord *game) const;

    int platformCount() const;
    int fileAssociationCount() const;
    int gameCount() const;
    QString indexedAt() const;

private:
    QSqlDatabase m_database;
    QString m_connectionName;
    QString m_lastError;

    bool createSchema();
    bool setError(const QString &message);
};
