
#include "DatabaseManager.h"

#include <QFileInfo>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

DatabaseManager::DatabaseManager(QObject *parent)
: QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    if (m_database.isOpen())
        m_database.close();
}

bool DatabaseManager::initialize(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    m_database = QSqlDatabase::addDatabase(
        "QSQLITE",
        "RecompStationConnection"
    );

    m_database.setDatabaseName(path);

    if (!m_database.open()) {
        m_lastError = m_database.lastError().text();
        return false;
    }

    // Enable foreign key enforcement.
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

    // Create database tables first.

    const QStringList statements = {

        R"(
            CREATE TABLE IF NOT EXISTS platforms (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL UNIQUE
            )
        )",

        R"(
            CREATE TABLE IF NOT EXISTS launchers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                platform_id INTEGER,
                executable_path TEXT NOT NULL,
                repository TEXT,
                FOREIGN KEY(platform_id)
                    REFERENCES platforms(id)
            )
        )",

        R"(
            CREATE TABLE IF NOT EXISTS games (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL,
                platform_id INTEGER NOT NULL,
                game_path TEXT,
                artwork_path TEXT,
                launcher_id INTEGER,
                FOREIGN KEY(platform_id)
                    REFERENCES platforms(id),
                FOREIGN KEY(launcher_id)
                    REFERENCES launchers(id)
            )
        )"
    };

    for (const QString &statement : statements) {

        if (!query.exec(statement)) {
            m_lastError = query.lastError().text();
            return false;
        }
    }

    // Register supported platforms after table creation.

    const QStringList platforms = {
        "N64",
        "PS1",
        "GameCube",
        "Wii"
    };

    for (const QString &platform : platforms) {

        QSqlQuery insertQuery(m_database);

        insertQuery.prepare(
            "INSERT OR IGNORE INTO platforms (name) "
            "VALUES (:name)"
        );

        insertQuery.bindValue(":name", platform);

        if (!insertQuery.exec()) {
            m_lastError = insertQuery.lastError().text();
            return false;
        }
    }

    return true;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}
