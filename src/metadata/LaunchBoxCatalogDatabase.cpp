#include "LaunchBoxCatalogDatabase.h"

#include "LaunchBoxCatalogMatcher.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <atomic>
#include <functional>

namespace {

QString platformKey(const QString &value)
{
    QString result;
    for (const QChar character : value) {
        if (character.isLetterOrNumber())
            result.append(character.toCaseFolded());
    }
    return result;
}

LaunchBoxGameRecord gameFromQuery(const QSqlQuery &query)
{
    LaunchBoxGameRecord game;
    game.databaseId = query.value(0).toInt();
    game.name = query.value(1).toString();
    game.platform = query.value(2).toString();
    game.releaseDate = QDate::fromString(query.value(3).toString(), Qt::ISODate);
    game.releaseYear = query.value(4).toInt();
    game.overview = query.value(5).toString();
    game.maxPlayers = query.value(6).toString();
    game.releaseType = query.value(7).toString();
    game.cooperative = query.value(8).toString();
    game.videoUrl = query.value(9).toString();
    game.communityRating = query.value(10).toString();
    game.wikipediaUrl = query.value(11).toString();
    game.esrb = query.value(12).toString();
    game.communityRatingCount = query.value(13).toString();
    game.genres = query.value(14).toString();
    game.developer = query.value(15).toString();
    game.publisher = query.value(16).toString();
    return game;
}

const QString gameSelect = QStringLiteral(
    "SELECT database_id, name, platform, release_date, release_year, overview, "
    "max_players, release_type, cooperative, video_url, community_rating, "
    "wikipedia_url, esrb, community_rating_count, genres, developer, publisher "
    "FROM launchbox_games ");

} // namespace

LaunchBoxCatalogDatabase::LaunchBoxCatalogDatabase()
    : m_connectionName(QStringLiteral("LaunchBoxCatalog_%1")
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

LaunchBoxCatalogDatabase::~LaunchBoxCatalogDatabase()
{
    close();
}

bool LaunchBoxCatalogDatabase::open(const QString &databasePath)
{
    close();
    if (!QDir().mkpath(QFileInfo(databasePath).absolutePath()))
        return setError(QStringLiteral("Could not create the LaunchBox catalog directory."));

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(databasePath);
    if (!m_database.open())
        return setError(m_database.lastError().text());

    QSqlQuery pragma(m_database);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON")))
        return setError(pragma.lastError().text());
    if (!pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL")))
        return setError(pragma.lastError().text());
    if (!createSchema())
        return false;
    m_lastError.clear();
    return true;
}

void LaunchBoxCatalogDatabase::close()
{
    if (!m_database.isValid())
        return;
    const QString name = m_connectionName;
    m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(name);
}

bool LaunchBoxCatalogDatabase::isOpen() const
{
    return m_database.isOpen();
}

QString LaunchBoxCatalogDatabase::lastError() const
{
    return m_lastError;
}

bool LaunchBoxCatalogDatabase::setError(const QString &message)
{
    m_lastError = message;
    return false;
}

bool LaunchBoxCatalogDatabase::createSchema()
{
    QSqlQuery query(m_database);
    const QStringList statements = {
        QStringLiteral(R"(CREATE TABLE IF NOT EXISTS launchbox_platforms (
            name TEXT PRIMARY KEY,
            normalized_name TEXT NOT NULL
        ))"),
        QStringLiteral(R"(CREATE TABLE IF NOT EXISTS launchbox_platform_aliases (
            platform_name TEXT NOT NULL,
            alias TEXT NOT NULL,
            normalized_alias TEXT NOT NULL,
            UNIQUE(platform_name, normalized_alias),
            FOREIGN KEY(platform_name) REFERENCES launchbox_platforms(name)
                ON DELETE CASCADE
        ))"),
        QStringLiteral(R"(CREATE TABLE IF NOT EXISTS launchbox_file_associations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            platform TEXT NOT NULL,
            platform_key TEXT NOT NULL,
            file_name TEXT NOT NULL,
            file_key TEXT NOT NULL,
            game_name TEXT NOT NULL,
            UNIQUE(platform_key, file_key, game_name)
        ))"),
        QStringLiteral(R"(CREATE TABLE IF NOT EXISTS launchbox_games (
            database_id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            platform TEXT NOT NULL,
            platform_key TEXT NOT NULL,
            title_key TEXT NOT NULL,
            release_date TEXT,
            release_year INTEGER NOT NULL DEFAULT 0,
            overview TEXT,
            max_players TEXT,
            release_type TEXT,
            cooperative TEXT,
            video_url TEXT,
            community_rating TEXT,
            wikipedia_url TEXT,
            esrb TEXT,
            community_rating_count TEXT,
            genres TEXT,
            developer TEXT,
            publisher TEXT
        ))"),
        QStringLiteral(R"(CREATE TABLE IF NOT EXISTS launchbox_game_images (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            database_id INTEGER NOT NULL,
            file_name TEXT NOT NULL,
            type TEXT NOT NULL,
            region TEXT NOT NULL DEFAULT '',
            crc32 TEXT NOT NULL DEFAULT '',
            UNIQUE(database_id, file_name, type, region)
        ))"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_launchbox_images_lookup "
                       "ON launchbox_game_images(database_id, type, region)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_launchbox_files_lookup "
                       "ON launchbox_file_associations(platform_key, file_key)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_launchbox_games_title "
                       "ON launchbox_games(platform_key, title_key)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_launchbox_games_platform "
                       "ON launchbox_games(platform_key, name COLLATE NOCASE)"),
        QStringLiteral(R"(CREATE TABLE IF NOT EXISTS launchbox_catalog_state (
            id INTEGER PRIMARY KEY CHECK(id = 1),
            indexed_at TEXT NOT NULL,
            platform_count INTEGER NOT NULL,
            file_count INTEGER NOT NULL,
            game_count INTEGER NOT NULL
        ))")
    };
    for (const QString &statement : statements) {
        if (!query.exec(statement))
            return setError(query.lastError().text());
    }
    return true;
}

bool LaunchBoxCatalogDatabase::rebuildFromXmlFiles(
    const QString &platformsXmlPath,
    const QString &filesXmlPath,
    const QString &metadataXmlPath,
    const std::atomic_bool *cancelled,
    const std::function<void(const QString &, int)> &progress)
{
    if (!m_database.isOpen())
        return setError(QStringLiteral("LaunchBox catalog database is not open."));

    const QStringList paths{platformsXmlPath, filesXmlPath, metadataXmlPath};
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.isFile() || !info.isReadable())
            return setError(QStringLiteral("LaunchBox catalog source is missing or unreadable: %1").arg(path));
    }

    if (!m_database.transaction())
        return setError(m_database.lastError().text());

    auto rollback = [this](const QString &message) {
        m_database.rollback();
        return setError(message);
    };

    QSqlQuery clear(m_database);
    const QStringList deleteStatements = {
        QStringLiteral("DELETE FROM launchbox_catalog_state"),
        QStringLiteral("DELETE FROM launchbox_file_associations"),
        QStringLiteral("DELETE FROM launchbox_game_images"),
        QStringLiteral("DELETE FROM launchbox_games"),
        QStringLiteral("DELETE FROM launchbox_platform_aliases"),
        QStringLiteral("DELETE FROM launchbox_platforms")
    };
    for (const QString &statement : deleteStatements) {
        if (!clear.exec(statement))
            return rollback(clear.lastError().text());
    }

    int platformsInserted = 0;
    int filesInserted = 0;
    int gamesInserted = 0;
    QString parseError;
    QString insertError;

    QSqlQuery insertPlatform(m_database);
    insertPlatform.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO launchbox_platforms(name, normalized_name) VALUES(?, ?)"));
    QSqlQuery insertAlias(m_database);
    insertAlias.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO launchbox_platform_aliases(platform_name, alias, normalized_alias) "
        "VALUES(?, ?, ?)"));

    if (progress) progress(QStringLiteral("Indexing platforms"), 0);
    const bool platformsOk = LaunchBoxXmlParser::parsePlatformsFile(
        platformsXmlPath,
        [&](const LaunchBoxPlatform &platform) {
            insertPlatform.bindValue(0, platform.name);
            insertPlatform.bindValue(1, platformKey(platform.name));
            if (!insertPlatform.exec()) {
                insertError = insertPlatform.lastError().text();
                return false;
            }
            if (insertPlatform.numRowsAffected() > 0)
                ++platformsInserted;
            for (const QString &alias : platform.alternateNames) {
                insertAlias.bindValue(0, platform.name);
                insertAlias.bindValue(1, alias);
                insertAlias.bindValue(2, platformKey(alias));
                if (!insertAlias.exec()) {
                    insertError = insertAlias.lastError().text();
                    return false;
                }
            }
            return true;
        }, &parseError, cancelled);
    if (!platformsOk)
        return rollback(insertError.isEmpty() ? parseError : insertError);

    if (progress) progress(QStringLiteral("Indexing file associations"), 33);
    QSqlQuery insertFile(m_database);
    insertFile.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO launchbox_file_associations"
        "(platform, platform_key, file_name, file_key, game_name) VALUES(?, ?, ?, ?, ?)"));
    const bool filesOk = LaunchBoxXmlParser::parseFilesFile(
        filesXmlPath,
        [&](const LaunchBoxFileAssociation &file) {
            insertFile.bindValue(0, file.platform);
            insertFile.bindValue(1, platformKey(file.platform));
            insertFile.bindValue(2, file.fileName);
            insertFile.bindValue(3, LaunchBoxCatalogMatcher::normalizedFileName(file.fileName));
            insertFile.bindValue(4, file.gameName);
            if (!insertFile.exec()) {
                insertError = insertFile.lastError().text();
                return false;
            }
            if (insertFile.numRowsAffected() > 0)
                ++filesInserted;
            return true;
        }, &parseError, cancelled);
    if (!filesOk)
        return rollback(insertError.isEmpty() ? parseError : insertError);

    if (progress) progress(QStringLiteral("Indexing game metadata"), 66);
    QSqlQuery insertGame(m_database);
    insertGame.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO launchbox_games("
        "database_id, name, platform, platform_key, title_key, release_date, release_year, "
        "overview, max_players, release_type, cooperative, video_url, community_rating, "
        "wikipedia_url, esrb, community_rating_count, genres, developer, publisher) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    const bool gamesOk = LaunchBoxXmlParser::parseMetadataFile(
        metadataXmlPath,
        [&](const LaunchBoxGameRecord &game) {
            insertGame.bindValue(0, game.databaseId);
            insertGame.bindValue(1, game.name);
            insertGame.bindValue(2, game.platform);
            insertGame.bindValue(3, platformKey(game.platform));
            insertGame.bindValue(4, LaunchBoxCatalogMatcher::normalizedTitle(game.name));
            insertGame.bindValue(5, game.releaseDate.isValid()
                                       ? game.releaseDate.toString(Qt::ISODate) : QString());
            insertGame.bindValue(6, game.releaseYear);
            insertGame.bindValue(7, game.overview);
            insertGame.bindValue(8, game.maxPlayers);
            insertGame.bindValue(9, game.releaseType);
            insertGame.bindValue(10, game.cooperative);
            insertGame.bindValue(11, game.videoUrl);
            insertGame.bindValue(12, game.communityRating);
            insertGame.bindValue(13, game.wikipediaUrl);
            insertGame.bindValue(14, game.esrb);
            insertGame.bindValue(15, game.communityRatingCount);
            insertGame.bindValue(16, game.genres);
            insertGame.bindValue(17, game.developer);
            insertGame.bindValue(18, game.publisher);
            if (!insertGame.exec()) {
                insertError = insertGame.lastError().text();
                return false;
            }
            if (insertGame.numRowsAffected() > 0)
                ++gamesInserted;
            return true;
        }, &parseError, cancelled);
    if (!gamesOk)
        return rollback(insertError.isEmpty() ? parseError : insertError);


    if (progress) progress(QStringLiteral("Indexing game images"), 82);
    QSqlQuery insertImage(m_database);
    insertImage.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO launchbox_game_images("
        "database_id, file_name, type, region, crc32) VALUES(?, ?, ?, ?, ?)"));
    const bool imagesOk = LaunchBoxXmlParser::parseGameImagesFile(
        metadataXmlPath,
        [&](const LaunchBoxGameImageRecord &image) {
            // Explicitly reset the prepared statement between streamed records.
            // This avoids retaining an active SQLite statement state while the
            // XML parser advances to the next image.
            insertImage.finish();
            insertImage.bindValue(0, image.databaseId);
            insertImage.bindValue(1, image.fileName);
            insertImage.bindValue(2, image.type);
            insertImage.bindValue(3, image.region);
            insertImage.bindValue(4, image.crc32);
            if (!insertImage.exec()) {
                insertError = insertImage.lastError().text();
                return false;
            }
            return true;
        }, &parseError, cancelled);
    if (!imagesOk)
        return rollback(insertError.isEmpty() ? parseError : insertError);

    if (cancelled && cancelled->load())
        return rollback(QStringLiteral("LaunchBox catalog update cancelled."));
    if (progress) progress(QStringLiteral("Finalizing catalog index"), 95);
    QSqlQuery state(m_database);
    state.prepare(QStringLiteral(
        "INSERT INTO launchbox_catalog_state"
        "(id, indexed_at, platform_count, file_count, game_count) "
        "VALUES(1, CURRENT_TIMESTAMP, ?, ?, ?)"));
    state.addBindValue(platformsInserted);
    state.addBindValue(filesInserted);
    state.addBindValue(gamesInserted);
    if (!state.exec())
        return rollback(state.lastError().text());

    if (!m_database.commit()) {
        const QString commitError = m_database.lastError().text();
        m_database.rollback();
        return setError(commitError);
    }
    m_lastError.clear();
    if (progress) progress(QStringLiteral("Catalog index complete"), 100);
    return true;
}

QList<LaunchBoxFileAssociation> LaunchBoxCatalogDatabase::findFileAssociations(
    const QString &internalPlatform, const QString &localFileName) const
{
    QList<LaunchBoxFileAssociation> result;
    if (!m_database.isOpen())
        return result;
    const QString canonical =
        LaunchBoxCatalogMatcher::canonicalPlatformForInternalName(internalPlatform);
    if (canonical.isEmpty())
        return result;

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT platform, file_name, game_name FROM launchbox_file_associations "
        "WHERE platform_key = ? AND file_key = ? ORDER BY id"));
    query.addBindValue(platformKey(canonical));
    query.addBindValue(LaunchBoxCatalogMatcher::normalizedFileName(localFileName));
    if (!query.exec())
        return result;
    while (query.next()) {
        result.append({query.value(0).toString(), query.value(1).toString(),
                       query.value(2).toString()});
    }
    return result;
}

QList<LaunchBoxGameRecord> LaunchBoxCatalogDatabase::findGamesByTitle(
    const QString &internalPlatform, const QString &title) const
{
    QList<LaunchBoxGameRecord> result;
    if (!m_database.isOpen())
        return result;
    const QString canonical =
        LaunchBoxCatalogMatcher::canonicalPlatformForInternalName(internalPlatform);
    if (canonical.isEmpty())
        return result;

    QSqlQuery query(m_database);
    query.prepare(gameSelect
        + QStringLiteral("WHERE platform_key = ? AND title_key = ? ORDER BY database_id"));
    query.addBindValue(platformKey(canonical));
    query.addBindValue(LaunchBoxCatalogMatcher::normalizedTitle(title));
    if (!query.exec())
        return result;
    while (query.next())
        result.append(gameFromQuery(query));
    return result;
}

QList<LaunchBoxGameRecord> LaunchBoxCatalogDatabase::findGamesByRevisionInsensitiveTitle(
    const QString &internalPlatform, const QString &title) const
{
    QList<LaunchBoxGameRecord> result;
    if (!m_database.isOpen())
        return result;
    const QString canonical =
        LaunchBoxCatalogMatcher::canonicalPlatformForInternalName(internalPlatform);
    if (canonical.isEmpty())
        return result;

    const QString exactKey = LaunchBoxCatalogMatcher::normalizedTitle(title);
    const QString revisionlessKey =
        LaunchBoxCatalogMatcher::normalizedTitleWithoutRevision(title);
    QSqlQuery query(m_database);
    query.prepare(gameSelect
        + QStringLiteral("WHERE platform_key = ? "
                         "AND (title_key = ? OR title_key = ? OR title_key LIKE ?) "
                         "ORDER BY database_id"));
    query.addBindValue(platformKey(canonical));
    query.addBindValue(exactKey);
    // Include the canonical title itself as well as revision-tagged variants.
    // The matcher prefers exact revisions and rejects competing candidates.
    query.addBindValue(revisionlessKey);
    query.addBindValue(revisionlessKey + QStringLiteral(" %"));
    if (!query.exec())
        return result;

    while (query.next())
        result.append(gameFromQuery(query));
    return result;
}

bool LaunchBoxCatalogDatabase::gameByDatabaseId(
    int databaseId, LaunchBoxGameRecord *game) const
{
    if (!game || !m_database.isOpen() || databaseId <= 0)
        return false;
    QSqlQuery query(m_database);
    query.prepare(gameSelect + QStringLiteral("WHERE database_id = ?"));
    query.addBindValue(databaseId);
    if (!query.exec() || !query.next())
        return false;
    *game = gameFromQuery(query);
    return true;
}


QList<LaunchBoxGameImageRecord> LaunchBoxCatalogDatabase::imagesByDatabaseId(
    int databaseId) const
{
    QList<LaunchBoxGameImageRecord> result;
    if (!m_database.isOpen() || databaseId <= 0)
        return result;

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT database_id, file_name, type, region, crc32 "
        "FROM launchbox_game_images WHERE database_id = ? "
        "ORDER BY type, region, file_name"));
    query.addBindValue(databaseId);
    if (!query.exec())
        return result;
    while (query.next()) {
        LaunchBoxGameImageRecord image;
        image.databaseId = query.value(0).toInt();
        image.fileName = query.value(1).toString();
        image.type = query.value(2).toString();
        image.region = query.value(3).toString();
        image.crc32 = query.value(4).toString();
        result.append(image);
    }
    return result;
}

int LaunchBoxCatalogDatabase::platformCount() const
{
    QSqlQuery query(m_database);
    return query.exec(QStringLiteral("SELECT COUNT(*) FROM launchbox_platforms"))
        && query.next() ? query.value(0).toInt() : 0;
}

int LaunchBoxCatalogDatabase::fileAssociationCount() const
{
    QSqlQuery query(m_database);
    return query.exec(QStringLiteral("SELECT COUNT(*) FROM launchbox_file_associations"))
        && query.next() ? query.value(0).toInt() : 0;
}

int LaunchBoxCatalogDatabase::gameCount() const
{
    QSqlQuery query(m_database);
    return query.exec(QStringLiteral("SELECT COUNT(*) FROM launchbox_games"))
        && query.next() ? query.value(0).toInt() : 0;
}


int LaunchBoxCatalogDatabase::gameImageCount() const
{
    QSqlQuery query(m_database);
    return query.exec(QStringLiteral("SELECT COUNT(*) FROM launchbox_game_images"))
        && query.next() ? query.value(0).toInt() : 0;
}

QString LaunchBoxCatalogDatabase::indexedAt() const
{
    QSqlQuery query(m_database);
    return query.exec(QStringLiteral("SELECT indexed_at FROM launchbox_catalog_state WHERE id = 1"))
        && query.next() ? query.value(0).toString() : QString();
}
