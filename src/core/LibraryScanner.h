#pragma once

#include <QObject>
#include <QString>

class PlatformManager;
class DatabaseManager;

class LibraryScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int lastScanCount READ lastScanCount NOTIFY scanCompleted)
    Q_PROPERTY(QString lastError READ lastError NOTIFY scanCompleted)

public:
    explicit LibraryScanner(
        PlatformManager *platformManager,
        DatabaseManager *databaseManager,
        QObject *parent = nullptr
    );

    int lastScanCount() const;
    QString lastError() const;

    Q_INVOKABLE int scanAll();
    Q_INVOKABLE int scanPlatform(const QString &platform);

signals:
    void scanCompleted(int gamesFound);

private:
    PlatformManager *m_platformManager;
    DatabaseManager *m_databaseManager;
    int m_lastScanCount = 0;
    QString m_lastError;

    QStringList supportedExtensions(const QString &platform) const;
    bool isDiscCompanionFile(const QString &filePath) const;
};
