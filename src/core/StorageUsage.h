#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QVariantMap>

// Measures how much space RecompStation's data folder uses, by area, on a
// background thread (launcher folders can hold many files). Used by
// Settings > Application.
class StorageUsage : public QObject
{
    Q_OBJECT
    // { launchers, artwork, launchBoxCatalog, downloads, database, logs, total } in bytes.
    Q_PROPERTY(QVariantMap usage READ usage NOTIFY usageChanged)
    Q_PROPERTY(bool measuring READ measuring NOTIFY measuringChanged)

public:
    explicit StorageUsage(const QString &dataDirectory, QObject *parent = nullptr);
    ~StorageUsage() override;

    QVariantMap usage() const;
    bool measuring() const;

    Q_INVOKABLE void refresh();
    // Removes leftover files from interrupted launcher downloads. The caller
    // must make sure no download or install is in progress.
    Q_INVOKABLE bool clearDownloadLeftovers();

    static qint64 directorySize(const QString &path);
    static QVariantMap measure(const QString &dataDirectory);

signals:
    void usageChanged();
    void measuringChanged();

private:
    QString m_dataDirectory;
    QVariantMap m_usage;
    QFutureWatcher<QVariantMap> m_watcher;
};
