#include "DatabaseManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {}

DatabaseManager::~DatabaseManager()
{
    if (m_database.isOpen()) m_database.close();
}

bool DatabaseManager::initialize(const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        m_lastError = "Unable to create database directory.";
        return false;
    }

    if (QSqlDatabase::contains("RecompStationConnection"))
        m_database = QSqlDatabase::database("RecompStationConnection");
    else
        m_database = QSqlDatabase::addDatabase("QSQLITE", "RecompStationConnection");

    m_database.setDatabaseName(path);
    if (!m_database.open()) {
        m_lastError = m_database.lastError().text();
        return false;
    }

    QSqlQuery pragma(m_database);
    if (!pragma.exec("PRAGMA foreign_keys = ON")) {
        m_lastError = pragma.lastError().text();
        return false;
    }
    return createTables();
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
        R"(CREATE TABLE IF NOT EXISTS game_metadata (
            game_id INTEGER PRIMARY KEY,
            provider TEXT NOT NULL,
            provider_game_id INTEGER NOT NULL,
            title TEXT NOT NULL,
            overview TEXT,
            release_date TEXT,
            provider_platform_id INTEGER,
            rating TEXT,
            FOREIGN KEY(game_id) REFERENCES games(id) ON DELETE CASCADE
        ))"
    };

    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            m_lastError = query.lastError().text();
            return false;
        }
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

    QSqlQuery platformQuery(m_database);
    platformQuery.prepare("SELECT id FROM platforms WHERE name = :name");
    platformQuery.bindValue(":name", platform);
    if (!platformQuery.exec() || !platformQuery.next()) {
        m_lastError = platformQuery.lastError().text();
        if (m_lastError.isEmpty()) m_lastError = "Platform is not registered: " + platform;
        return false;
    }

    QSqlQuery insertQuery(m_database);
    insertQuery.prepare(
        "INSERT OR IGNORE INTO games (title, platform_id, game_path) "
        "VALUES (:title, :platform_id, :game_path)"
    );
    insertQuery.bindValue(":title", title);
    insertQuery.bindValue(":platform_id", platformQuery.value(0));
    insertQuery.bindValue(":game_path", gamePath);
    if (!insertQuery.exec()) {
        m_lastError = insertQuery.lastError().text();
        return false;
    }
    m_lastError.clear();
    return insertQuery.numRowsAffected() > 0;
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
        "COALESCE(game_metadata.rating, '') "
        "FROM games "
        "JOIN platforms ON games.platform_id = platforms.id "
        "LEFT JOIN game_metadata ON game_metadata.game_id = games.id ";

    if (platform.isEmpty()) {
        query.prepare(select + "ORDER BY platforms.name, games.title COLLATE NOCASE");
    } else {
        query.prepare(select
            + "WHERE platforms.name = :platform "
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
                                        const QString &rating)
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
            release_date, provider_platform_id, rating
        ) VALUES (
            :game_id, :provider, :provider_game_id, :title, :overview,
            :release_date, :provider_platform_id, :rating
        )
        ON CONFLICT(game_id) DO UPDATE SET
            provider = excluded.provider,
            provider_game_id = excluded.provider_game_id,
            title = excluded.title,
            overview = excluded.overview,
            release_date = excluded.release_date,
            provider_platform_id = excluded.provider_platform_id,
            rating = excluded.rating
    )");
    metadataQuery.bindValue(":game_id", gameId);
    metadataQuery.bindValue(":provider", provider.trimmed());
    metadataQuery.bindValue(":provider_game_id", providerGameId);
    metadataQuery.bindValue(":title", metadataTitle.trimmed());
    metadataQuery.bindValue(":overview", overview);
    metadataQuery.bindValue(":release_date", releaseDate);
    metadataQuery.bindValue(":provider_platform_id", providerPlatformId);
    metadataQuery.bindValue(":rating", rating);

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
