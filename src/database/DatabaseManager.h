
#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    bool initialize(const QString &path);

    QString lastError() const;

private:
    QSqlDatabase m_database;
    QString m_lastError;

    bool createTables();
};
