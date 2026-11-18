#include "DatabaseManager.h"
#include "../core/Logging.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>
#include <QStringList>

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {}

DatabaseManager::~DatabaseManager()
{
    if (m_database.isOpen()) m_database.close();
}

bool DatabaseManager::initialize(const QString &path)
{
    qCInfo(lcDatabase) << "Initializing library database.";
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        m_lastError = "Unable to create database directory.";
        qCCritical(lcDatabase) << m_lastError;
        return false;
    }

    if (QSqlDatabase::contains("RecompStationConnection"))
        m_database = QSqlDatabase::database("RecompStationConnection");
    else
        m_database = QSqlDatabase::addDatabase("QSQLITE", "RecompStationConnection");

    m_database.setDatabaseName(path);
    if (!m_database.open()) {
        m_lastError = m_database.lastError().text();
        qCCritical(lcDatabase) << "Could not open database:" << m_lastError;
        return false;
    }

    QSqlQuery pragma(m_database);
    if (!pragma.exec("PRAGMA foreign_keys = ON")) {
        m_lastError = pragma.lastError().text();
        return false;
    }
    const bool initialized = createTables();
    if (initialized)
        qCInfo(lcDatabase) << "Database schema is ready.";
    else
        qCCritical(lcDatabase) << "Database schema initialization failed:" << m_lastError;
    return initialized;
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(m_database);
    const QStringList statements = {
        R"(CREATE TABLE IF NOT EXISTS platforms (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE
        ))",
        R"(CREATE TABLE IF NOT EXISTS launchers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            platform_id INTEGER,
            executable_path TEXT NOT NULL,
            repository TEXT,
            FOREIGN KEY(platform_id) REFERENCES platforms(id)
        ))",
        R"(CREATE TABLE IF NOT EXISTS games (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            platform_id INTEGER NOT NULL,
            game_path TEXT,
            artwork_path TEXT,
            launcher_id INTEGER,
            FOREIGN KEY(platform_id) REFERENCES platforms(id),
            FOREIGN KEY(launcher_id) REFERENCES launchers(id)
        ))",
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_games_game_path "
        "ON games(game_path) WHERE game_path IS NOT NULL",
        R"(CREATE TABLE IF NOT EXISTS metadata_lookup_attempts (
            game_id INTEGER PRIMARY KEY,
            status TEXT NOT NULL,
            attempted_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(game_id) REFERENCES games(id) ON DELETE CASCADE
        ))",
        R"(CREATE TABLE IF NOT EXISTS game_metadata (
            game_id INTEGER PRIMARY KEY,
            provider TEXT NOT NULL,
            provider_game_id INTEGER NOT NULL,
            title TEXT NOT NULL,
            overview TEXT,
            release_date TEXT,
            provider_platform_id INTEGER,
            rating TEXT,
            artwork_url TEXT,
            FOREIGN KEY(game_id) REFERENCES games(id) ON DELETE CASCADE
        ))",
        R"(CREATE TABLE IF NOT EXISTS game_screenshots (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            game_id INTEGER NOT NULL,
            local_path TEXT NOT NULL UNIQUE,
            FOREIGN KEY(game_id) REFERENCES games(id) ON DELETE CASCADE
        ))"
    };

    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            m_lastError = query.lastError().text();
            return false;
        }
    }

    // Upgrade existing libraries with a scan-availability flag.
    QSqlQuery gameColumns(m_database);
    if (!gameColumns.exec("PRAGMA table_info(games)")) {
        m_lastError = gameColumns.lastError().text();
        return false;
    }
    bool hasAvailability = false;
    while (gameColumns.next()) {
        if (gameColumns.value(1).toString() == QStringLiteral("is_available"))
            hasAvailability = true;
    }
    gameColumns.finish();
    if (!hasAvailability && !query.exec(
            "ALTER TABLE games ADD COLUMN is_available INTEGER NOT NULL DEFAULT 1")) {
        m_lastError = query.lastError().text();
        return false;
    }

    // Upgrade databases created before artwork URLs were stored.
    QSqlQuery columns(m_database);
    if (!columns.exec("PRAGMA table_info(game_metadata)")) {
        m_lastError = columns.lastError().text();
        return false;
    }
    bool hasArtworkUrl = false;
    while (columns.next()) {
        if (columns.value(1).toString() == QStringLiteral("artwork_url"))
            hasArtworkUrl = true;
    }
    columns.finish();
    if (!hasArtworkUrl && !query.exec(
            "ALTER TABLE game_metadata ADD COLUMN artwork_url TEXT")) {
        m_lastError = query.lastError().text();
        return false;
    }

    for (const QString &platform : QStringList{"N64", "PS1", "GameCube", "Wii"}) {
        QSqlQuery insertQuery(m_database);
        insertQuery.prepare("INSERT OR IGNORE INTO platforms (name) VALUES (:name)");
        insertQuery.bindValue(":name", platform);
        if (!insertQuery.exec()) {
            m_lastError = insertQuery.lastError().text();
            return false;
        }
    }
    return true;
}

bool DatabaseManager::registerGame(const QString &title,
                                   const QString &platform,
                                   const QString &gamePath)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not open.";
        return false;
    }

    if (!m_database.transaction()) {
        m_lastError = m_database.lastError().text();
        return false;
    }

    QSqlQuery platformQuery(m_database);
    platformQuery.prepare("SELECT id FROM platforms WHERE name = :name");
    platformQuery.bindValue(":name", platform);
    if (!platformQuery.exec() || !platformQuery.next()) {
        m_lastError = platformQuery.lastError().text();
        if (m_lastError.isEmpty())
            m_lastError = "Platform is not registered: " + platform;
        platformQuery.finish();
        m_database.rollback();
        return false;
    }
    const int platformId = platformQuery.value(0).toInt();
    platformQuery.finish();

    QSqlQuery existingQuery(m_database);
    existingQuery.prepare(
        "SELECT id, is_available FROM games WHERE game_path = :game_path");
    existingQuery.bindValue(":game_path", gamePath);
    if (!existingQuery.exec()) {
        m_lastError = existingQuery.lastError().text();
        m_database.rollback();
        return false;
    }
    const bool isReappearing = existingQuery.next()
        && existingQuery.value(1).toInt() == 0;
    const int existingGameId = isReappearing
        ? existingQuery.value(0).toInt() : 0;
    existingQuery.finish();

    QSqlQuery insertQuery(m_database);
    insertQuery.prepare(
        "INSERT INTO games (title, platform_id, game_path, is_available) "
        "VALUES (:title, :platform_id, :game_path, 1) "
        "ON CONFLICT(game_path) WHERE game_path IS NOT NULL DO UPDATE SET "
        "title = excluded.title, platform_id = excluded.platform_id, "
        "is_available = 1 "
        "WHERE games.is_available = 0 "
        "OR games.title <> excluded.title "
        "OR games.platform_id <> excluded.platform_id"
    );
    insertQuery.bindValue(":title", title);
    insertQuery.bindValue(":platform_id", platformId);
    insertQuery.bindValue(":game_path", gamePath);
    if (!insertQuery.exec()) {
        m_lastError = insertQuery.lastError().text();
        m_database.rollback();
        return false;
    }
    const bool changed = insertQuery.numRowsAffected() > 0;
    insertQuery.finish();

    // A removed ROM keeps its database row and metadata. If it returns
    // without an associated metadata record, let automatic matching try
    // again instead of carrying forward an obsolete no-match/ambiguous state.
    if (isReappearing && existingGameId > 0) {
        QSqlQuery resetLookup(m_database);
        resetLookup.prepare(R"(
            DELETE FROM metadata_lookup_attempts
            WHERE game_id = :game_id
              AND NOT EXISTS (
                  SELECT 1 FROM game_metadata
                  WHERE game_metadata.game_id = :game_id
              )
        )");
        resetLookup.bindValue(":game_id", existingGameId);
        if (!resetLookup.exec()) {
            m_lastError = resetLookup.lastError().text();
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit()) {
        m_lastError = m_database.lastError().text();
        return false;
    }
    m_lastError.clear();
    return changed;
}

bool DatabaseManager::reconcilePlatformGames(
    const QString &platform, const QStringList &discoveredPaths)
{
    if (!m_database.isOpen()) {
        m_lastError = QStringLiteral("Database is not open.");
        return false;
    }

    QSqlQuery platformQuery(m_database);
    platformQuery.prepare("SELECT id FROM platforms WHERE name = :name");
    platformQuery.bindValue(":name", platform);
    if (!platformQuery.exec() || !platformQuery.next()) {
        m_lastError = platformQuery.lastError().text();
        if (m_lastError.isEmpty())
            m_lastError = QStringLiteral("Platform is not registered: ") + platform;
        return false;
    }
    const int platformId = platformQuery.value(0).toInt();

    if (!m_database.transaction()) {
        m_lastError = m_database.lastError().text();
        return false;
    }

    QSqlQuery markMissing(m_database);
    markMissing.prepare(
        "UPDATE games SET is_available = 0 "
        "WHERE platform_id = :platform_id");
    markMissing.bindValue(":platform_id", platformId);
    if (!markMissing.exec()) {
        m_lastError = markMissing.lastError().text();
        m_database.rollback();
        return false;
    }

    QSqlQuery markFound(m_database);
    markFound.prepare(
        "UPDATE games SET is_available = 1 "
        "WHERE platform_id = :platform_id AND game_path = :game_path");
    for (const QString &path : discoveredPaths) {
        markFound.bindValue(":platform_id", platformId);
        markFound.bindValue(":game_path", path);
        if (!markFound.exec()) {
            m_lastError = markFound.lastError().text();
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit()) {
        m_lastError = m_database.lastError().text();
        return false;
    }
    m_lastError.clear();
    return true;
}

QVariantList DatabaseManager::games(const QString &platform) const
{
    QVariantList result;
    if (!m_database.isOpen()) return result;

    QSqlQuery query(m_database);
    const QString select =
        "SELECT games.id, games.title, platforms.name, games.game_path, "
        "COALESCE(game_metadata.provider, ''), "
        "COALESCE(game_metadata.provider_game_id, 0), "
        "COALESCE(game_metadata.title, ''), "
        "COALESCE(game_metadata.overview, ''), "
        "COALESCE(game_metadata.release_date, ''), "
        "COALESCE(game_metadata.provider_platform_id, 0), "
        "COALESCE(game_metadata.rating, ''), "
        "COALESCE(games.artwork_path, ''), "
        "COALESCE(game_metadata.artwork_url, '') "
        "FROM games "
        "JOIN platforms ON games.platform_id = platforms.id "
        "LEFT JOIN game_metadata ON game_metadata.game_id = games.id "
        "WHERE games.is_available = 1 ";

    if (platform.isEmpty()) {
        query.prepare(select + "ORDER BY platforms.name, games.title COLLATE NOCASE");
    } else {
        query.prepare(select
            + "AND platforms.name = :platform "
              "ORDER BY games.title COLLATE NOCASE");
        query.bindValue(":platform", platform);
    }

    if (!query.exec()) return result;
    while (query.next()) {
        QVariantMap game;
        const int id = query.value(0).toInt();
        const QString title = query.value(1).toString();
        const QString platformName = query.value(2).toString();
        const QString path = query.value(3).toString();
        const QString metadataTitle = query.value(6).toString();
        game.insert("id", id);
        game.insert("title", title);
        game.insert("platform", platformName);
        game.insert("path", path);
        game.insert("label", title + " (" + platformName + ") — " + path
                    + (metadataTitle.isEmpty()
                           ? QString()
                           : "  •  Metadata: " + metadataTitle));
        game.insert("metadataProvider", query.value(4).toString());
        game.insert("metadataGameId", query.value(5).toInt());
        game.insert("metadataTitle", query.value(6).toString());
        game.insert("metadataOverview", query.value(7).toString());
        game.insert("metadataReleaseDate", query.value(8).toString());
        game.insert("metadataPlatformId", query.value(9).toInt());
        game.insert("metadataRating", query.value(10).toString());
        const QString artworkPath = query.value(11).toString();
        game.insert("artworkPath", artworkPath.isEmpty()
                    ? QString() : QUrl::fromLocalFile(artworkPath).toString());
        game.insert("metadataArtworkUrl", query.value(12).toString());
        game.insert("screenshots", gameScreenshots(id));
        result.append(game);
    }
    return result;
}

bool DatabaseManager::associateMetadata(int gameId,
                                        const QString &provider,
                                        int providerGameId,
                                        const QString &metadataTitle,
                                        const QString &overview,
                                        const QString &releaseDate,
                                        int providerPlatformId,
                                        const QString &rating,
                                        const QString &artworkUrl)
{
    auto fail = [this](const QString &message) {
        m_lastError = message;
        emit errorChanged();
        return false;
    };

    if (!m_database.isOpen())
        return fail("Database is not open.");
    if (gameId <= 0 || provider.trimmed().isEmpty()
        || providerGameId <= 0 || metadataTitle.trimmed().isEmpty()) {
        return fail("A local game and valid metadata result are required.");
    }

    QSqlQuery gameQuery(m_database);
    gameQuery.prepare("SELECT id FROM games WHERE id = :id");
    gameQuery.bindValue(":id", gameId);
    if (!gameQuery.exec())
        return fail(gameQuery.lastError().text());
    if (!gameQuery.next())
        return fail("The selected local game no longer exists.");
    gameQuery.finish();

    if (!m_database.transaction())
        return fail(m_database.lastError().text());

    QSqlQuery metadataQuery(m_database);
    metadataQuery.prepare(R"(
        INSERT INTO game_metadata (
            game_id, provider, provider_game_id, title, overview,
            release_date, provider_platform_id, rating, artwork_url
        ) VALUES (
            :game_id, :provider, :provider_game_id, :title, :overview,
            :release_date, :provider_platform_id, :rating, :artwork_url
        )
        ON CONFLICT(game_id) DO UPDATE SET
            provider = excluded.provider,
            provider_game_id = excluded.provider_game_id,
            title = excluded.title,
            overview = excluded.overview,
            release_date = excluded.release_date,
            provider_platform_id = excluded.provider_platform_id,
            rating = excluded.rating,
            artwork_url = excluded.artwork_url
    )");
    metadataQuery.bindValue(":game_id", gameId);
    metadataQuery.bindValue(":provider", provider.trimmed());
    metadataQuery.bindValue(":provider_game_id", providerGameId);
    metadataQuery.bindValue(":title", metadataTitle.trimmed());
    metadataQuery.bindValue(":overview", overview);
    metadataQuery.bindValue(":release_date", releaseDate);
    metadataQuery.bindValue(":provider_platform_id", providerPlatformId);
    metadataQuery.bindValue(":rating", rating);
    metadataQuery.bindValue(":artwork_url", artworkUrl);

    if (!metadataQuery.exec()) {
        const QString error = metadataQuery.lastError().text();
        m_database.rollback();
        return fail(error);
    }
    if (!m_database.commit())
        return fail(m_database.lastError().text());

    m_lastError.clear();
    emit errorChanged();
    return true;
}

QString DatabaseManager::lastError() const { return m_lastError; }

bool DatabaseManager::setArtworkPath(int gameId, const QString &artworkPath)
{
    if (!m_database.isOpen() || gameId <= 0 || artworkPath.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("A valid local game and artwork path are required.");
        emit errorChanged();
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("UPDATE games SET artwork_path = :path WHERE id = :id");
    query.bindValue(":path", artworkPath);
    query.bindValue(":id", gameId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        m_lastError = query.lastError().text();
        if (m_lastError.isEmpty())
            m_lastError = QStringLiteral("The selected local game no longer exists.");
        emit errorChanged();
        return false;
    }
    m_lastError.clear();
    emit errorChanged();
    return true;
}


QVariantList DatabaseManager::gamesNeedingMetadata(bool includePreviouslyChecked) const
{
    QVariantList result;
    if (!m_database.isOpen())
        return result;

    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT games.id, games.title, platforms.name,
               CASE WHEN game_metadata.game_id IS NULL THEN 0 ELSE 1 END,
               COALESCE(game_metadata.artwork_url, '')
        FROM games
        JOIN platforms ON games.platform_id = platforms.id
        LEFT JOIN game_metadata ON game_metadata.game_id = games.id
        LEFT JOIN metadata_lookup_attempts
            ON metadata_lookup_attempts.game_id = games.id
        WHERE games.is_available = 1 AND (
            (game_metadata.game_id IS NULL
             AND (metadata_lookup_attempts.game_id IS NULL OR :retry = 1))
            OR
            (:retry = 1 AND game_metadata.game_id IS NOT NULL
             AND (games.artwork_path IS NULL OR games.artwork_path = '')
             )
        )
        ORDER BY platforms.name, games.title COLLATE NOCASE
    )");
    query.bindValue(QStringLiteral(":retry"), includePreviouslyChecked ? 1 : 0);
    if (!query.exec())
        return result;

    while (query.next()) {
        result.append(QVariantMap{
            {QStringLiteral("id"), query.value(0).toInt()},
            {QStringLiteral("title"), query.value(1).toString()},
            {QStringLiteral("platform"), query.value(2).toString()},
            {QStringLiteral("hasMetadata"), query.value(3).toBool()},
            {QStringLiteral("artworkUrl"), query.value(4).toString()}
        });
    }
    return result;
}

bool DatabaseManager::markMetadataLookupAttempt(int gameId, const QString &status)
{
    const QStringList allowedStatuses{
        QStringLiteral("matched"),
        QStringLiteral("no_match"),
        QStringLiteral("ambiguous"),
        QStringLiteral("provider_error")
    };
    if (!m_database.isOpen() || gameId <= 0 || !allowedStatuses.contains(status)) {
        m_lastError = QStringLiteral("A valid game and metadata lookup status are required.");
        emit errorChanged();
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO metadata_lookup_attempts (game_id, status, attempted_at)
        VALUES (:game_id, :status, CURRENT_TIMESTAMP)
        ON CONFLICT(game_id) DO UPDATE SET
            status = excluded.status,
            attempted_at = excluded.attempted_at
    )");
    query.bindValue(QStringLiteral(":game_id"), gameId);
    query.bindValue(QStringLiteral(":status"), status);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        emit errorChanged();
        return false;
    }
    m_lastError.clear();
    emit errorChanged();
    return true;
}

QVariantList DatabaseManager::gamesWithMetadata() const
{
    QVariantList result;
    if (!m_database.isOpen())
        return result;
    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT games.id, game_metadata.provider, game_metadata.provider_game_id
        FROM games
        JOIN game_metadata ON game_metadata.game_id = games.id
        WHERE games.is_available = 1
        ORDER BY games.id
    )");
    if (!query.exec())
        return result;
    while (query.next()) {
        result.append(QVariantMap{
            {QStringLiteral("id"), query.value(0).toInt()},
            {QStringLiteral("metadataProvider"), query.value(1).toString()},
            {QStringLiteral("metadataGameId"), query.value(2).toInt()}
        });
    }
    return result;
}

bool DatabaseManager::addGameScreenshot(int gameId, const QString &localPath)
{
    if (!m_database.isOpen() || gameId <= 0 || localPath.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("A valid game and screenshot path are required.");
        emit errorChanged();
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare("INSERT OR IGNORE INTO game_screenshots (game_id, local_path) VALUES (:game_id, :path)");
    query.bindValue(":game_id", gameId);
    query.bindValue(":path", localPath);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        emit errorChanged();
        return false;
    }
    m_lastError.clear();
    emit errorChanged();
    return true;
}

bool DatabaseManager::clearGameScreenshots(int gameId)
{
    if (!m_database.isOpen() || gameId <= 0) {
        m_lastError = QStringLiteral("A valid local game is required.");
        emit errorChanged();
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM game_screenshots WHERE game_id = :game_id");
    query.bindValue(":game_id", gameId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        emit errorChanged();
        return false;
    }
    m_lastError.clear();
    emit errorChanged();
    return true;
}

QVariantList DatabaseManager::gameScreenshots(int gameId) const
{
    QVariantList result;
    if (!m_database.isOpen() || gameId <= 0)
        return result;
    QSqlQuery query(m_database);
    query.prepare("SELECT local_path FROM game_screenshots WHERE game_id = :game_id ORDER BY id");
    query.bindValue(":game_id", gameId);
    if (!query.exec())
        return result;
    while (query.next()) {
        const QString path = query.value(0).toString();
        if (QFileInfo::exists(path))
            result.append(QUrl::fromLocalFile(path).toString());
    }
    return result;
}
