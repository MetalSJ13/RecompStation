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
        "ON games(game_path) WHERE game_path IS NOT NULL"
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
    if (platform.isEmpty()) {
        query.prepare(
            "SELECT games.title, platforms.name, games.game_path "
            "FROM games JOIN platforms ON games.platform_id = platforms.id "
            "ORDER BY platforms.name, games.title COLLATE NOCASE"
        );
    } else {
        query.prepare(
            "SELECT games.title, platforms.name, games.game_path "
            "FROM games JOIN platforms ON games.platform_id = platforms.id "
            "WHERE platforms.name = :platform ORDER BY games.title COLLATE NOCASE"
        );
        query.bindValue(":platform", platform);
    }

    if (!query.exec()) return result;
    while (query.next()) {
        QVariantMap game;
        game.insert("title", query.value(0).toString());
        game.insert("platform", query.value(1).toString());
        game.insert("path", query.value(2).toString());
        result.append(game);
    }
    return result;
}

QString DatabaseManager::lastError() const { return m_lastError; }
