#include "StorageUsage.h"
#include "Logging.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>

StorageUsage::StorageUsage(const QString &dataDirectory, QObject *parent)
    : QObject(parent),
      m_dataDirectory(dataDirectory)
{
    connect(&m_watcher, &QFutureWatcher<QVariantMap>::finished, this, [this]() {
        m_usage = m_watcher.result();
        emit usageChanged();
        emit measuringChanged();
    });
}

StorageUsage::~StorageUsage()
{
    if (m_watcher.isRunning())
        m_watcher.waitForFinished();
}

QVariantMap StorageUsage::usage() const { return m_usage; }
bool StorageUsage::measuring() const { return m_watcher.isRunning(); }

void StorageUsage::refresh()
{
    if (m_watcher.isRunning())
        return;
    const QString directory = m_dataDirectory;
    m_watcher.setFuture(QtConcurrent::run([directory]() { return measure(directory); }));
    emit measuringChanged();
}

qint64 StorageUsage::directorySize(const QString &path)
{
    qint64 total = 0;
    // Symlinks are not followed, so a link to elsewhere is not counted.
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

QVariantMap StorageUsage::measure(const QString &dataDirectory)
{
    const QDir root(dataDirectory);
    QVariantMap usage;
    qint64 total = 0;
    const QList<QPair<QString, QString>> areas = {
        {QStringLiteral("launchers"), QStringLiteral("launchers")},
        {QStringLiteral("artwork"), QStringLiteral("artwork")},
        {QStringLiteral("launchBoxCatalog"), QStringLiteral("metadata/launchbox")},
        {QStringLiteral("downloads"), QStringLiteral("downloads")},
        {QStringLiteral("database"), QStringLiteral("database")},
        {QStringLiteral("logs"), QStringLiteral("logs")},
    };
    for (const auto &area : areas) {
        const qint64 size = directorySize(root.filePath(area.second));
        usage.insert(area.first, size);
        total += size;
    }
    usage.insert(QStringLiteral("total"), total);
    return usage;
}

bool StorageUsage::clearDownloadLeftovers()
{
    const QString staging = QDir(m_dataDirectory).filePath(QStringLiteral("downloads/staging"));
    QDir dir(staging);
    if (!dir.exists())
        return true;
    bool ok = true;
    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir() && !entry.isSymLink())
            ok = QDir(entry.absoluteFilePath()).removeRecursively() && ok;
        else
            ok = QFile::remove(entry.absoluteFilePath()) && ok;
    }
    qCInfo(lcApplication) << "Cleared leftover launcher downloads:" << (ok ? "done" : "partly");
    refresh();
    return ok;
}
