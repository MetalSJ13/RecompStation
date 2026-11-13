#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    bool initialize(const QString &path);
    QString lastError() const;

    Q_INVOKABLE bool registerGame(const QString &title,
                                  const QString &platform,
                                  const QString &gamePath);
    Q_INVOKABLE QVariantList games(const QString &platform = QString()) const;

private:
    QSqlDatabase m_database;
    QString m_lastError;
    bool createTables();
};
