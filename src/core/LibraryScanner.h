#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileSystemWatcher>
#include <QTimer>

class PlatformManager;
class DatabaseManager;

class LibraryScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int lastScanCount READ lastScanCount NOTIFY scanCompleted)
    Q_PROPERTY(QString lastError READ lastError NOTIFY scanCompleted)
    Q_PROPERTY(bool monitoringActive READ monitoringActive NOTIFY monitoringActiveChanged)

public:
    explicit LibraryScanner(
        PlatformManager *platformManager,
        DatabaseManager *databaseManager,
        QObject *parent = nullptr
    );

    int lastScanCount() const;
    QString lastError() const;
    bool monitoringActive() const;

    Q_INVOKABLE int scanAll();
    Q_INVOKABLE int scanPlatform(const QString &platform);
    Q_INVOKABLE bool startMonitoring();
    Q_INVOKABLE void stopMonitoring();

signals:
    void scanCompleted(int gamesFound);
    void monitoringActiveChanged();

private:
    static constexpr int WatchDebounceMs = 750;

    PlatformManager *m_platformManager;
    DatabaseManager *m_databaseManager;
    QFileSystemWatcher m_fileWatcher;
    QTimer m_watchDebounceTimer;
    int m_lastScanCount = 0;
    QString m_lastError;
    bool m_scanningAll = false;
    bool m_monitoringActive = false;

    QStringList supportedExtensions(const QString &platform) const;
    bool isDiscCompanionFile(const QString &filePath) const;
    void scheduleAutomaticScan(const QString &changedPath);
    void refreshDirectoryWatches();
};