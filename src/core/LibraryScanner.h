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
    Q_PROPERTY(int lastUnsupportedCount READ lastUnsupportedCount NOTIFY scanCompleted)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY scanCompleted)
    Q_PROPERTY(bool monitoringActive READ monitoringActive NOTIFY monitoringActiveChanged)

public:
    explicit LibraryScanner(
        PlatformManager *platformManager,
        DatabaseManager *databaseManager,
        QObject *parent = nullptr
    );

    int lastScanCount() const;
    int lastUnsupportedCount() const;
    bool scanning() const;
    QString lastError() const;
    bool monitoringActive() const;

    Q_INVOKABLE int scanAll();
    Q_INVOKABLE bool scanAllAsync();
    Q_INVOKABLE int scanPlatform(const QString &platform);
    Q_INVOKABLE bool startMonitoring();
    Q_INVOKABLE void stopMonitoring();
    // How long file changes settle before a rescan (default WatchDebounceMs).
    // Tests shorten it.
    void setWatchDebounceInterval(int milliseconds);

signals:
    void scanStarted();
    void platformScanStarted(const QString &platform, int platformIndex, int platformCount);
    void platformScanCompleted(const QString &platform, int gamesFound, int unsupportedFiles);
    void scanCompleted(int gamesFound);
    void scanStateChanged();
    void monitoringActiveChanged();

private:
    void unpackArchivedRoms(const QString &platform, const QString &directoryPath,
                            const QStringList &extensions);
    static constexpr int WatchDebounceMs = 750;

    PlatformManager *m_platformManager;
    DatabaseManager *m_databaseManager;
    QFileSystemWatcher m_fileWatcher;
    QTimer m_watchDebounceTimer;
    int m_lastScanCount = 0;
    int m_lastUnsupportedCount = 0;
    int m_asyncPlatformIndex = 0;
    int m_asyncTotalFound = 0;
    int m_asyncUnsupportedCount = 0;
    QStringList m_asyncErrors;
    QStringList m_asyncPlatforms;
    bool m_asyncScanActive = false;
    QString m_lastError;
    bool m_scanningAll = false;
    bool m_monitoringActive = false;
    // Set after the first watches of a monitoring session are in place.
    bool m_watchesStarted = false;

    QStringList supportedExtensions(const QString &platform) const;
    bool isDiscCompanionFile(const QString &filePath) const;
    void scheduleAutomaticScan(const QString &changedPath);
    void refreshDirectoryWatches();
    void scanNextPlatformAsync();
};