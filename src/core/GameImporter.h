#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <atomic>
#include <memory>

class PlatformManager;

// Finds game files in folders the user points at (or files dropped on the
// window), works out which system each belongs to, and copies or moves the
// selected ones into the library's platform folders. Existing files are never
// overwritten. N64 ROMs inside .zip / .7z archives are unpacked.
class GameImporter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool importing READ importing NOTIFY importingChanged)
    // Each entry: { path, files, name, platform, sure, selected, size,
    //               archiveEntry, exists }
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY candidatesChanged)
    Q_PROPERTY(int completedCount READ completedCount NOTIFY progressChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesDone READ bytesDone NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesTotal READ bytesTotal NOTIFY progressChanged)
    Q_PROPERTY(QString currentName READ currentName NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit GameImporter(PlatformManager *platformManager, QObject *parent = nullptr);
    ~GameImporter() override;

    QStringList sources() const;
    bool scanning() const;
    bool importing() const;
    QVariantList candidates() const;
    int completedCount() const;
    int totalCount() const;
    qint64 bytesDone() const;
    qint64 bytesTotal() const;
    QString currentName() const;
    QString status() const;

    // Paths or file:// URLs; folders are searched, files are checked directly.
    Q_INVOKABLE void addSource(const QString &pathOrUrl);
    Q_INVOKABLE void removeSource(const QString &path);
    Q_INVOKABLE void setSources(const QVariantList &pathsOrUrls);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setCandidateSelected(int index, bool selected);
    Q_INVOKABLE void setCandidatePlatform(int index, const QString &platform);
    Q_INVOKABLE int selectedCount() const;
    Q_INVOKABLE qint64 selectedBytes() const;
    Q_INVOKABLE qint64 libraryFreeBytes() const;
    Q_INVOKABLE void importSelected(bool move);
    Q_INVOKABLE void cancel();

    // System for a single file, one of PlatformManager's platforms, or
    // empty. *sure is false when the file could belong to another system.
    static QString detectPlatform(const QString &filePath, bool *sure);
    // Searches files and folders. Skips anything inside excludedRoot.
    static QVariantList findCandidates(const QStringList &paths, const QString &excludedRoot,
                                       const std::atomic_bool *cancelled = nullptr);
    static QString localPath(const QString &pathOrUrl);

signals:
    void sourcesChanged();
    void scanningChanged();
    void importingChanged();
    void candidatesChanged();
    void progressChanged();
    void statusChanged();
    void importFinished(int imported, int skipped, int failed);

private:
    struct ImportResult { int imported = 0; int skipped = 0; int failed = 0; QStringList importedPaths; };

    void startScan();
    void markExisting();
    void setStatus(const QString &status);
    ImportResult runImport(const QVariantList &items, bool move);

    PlatformManager *m_platformManager = nullptr;
    QStringList m_sources;
    QVariantList m_candidates;
    QFutureWatcher<QVariantList> m_scanWatcher;
    QFutureWatcher<ImportResult> m_importWatcher;
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_rescanRequested = false;
    std::atomic<int> m_completed{0};
    int m_total = 0;
    std::atomic<qint64> m_bytesDone{0};
    qint64 m_bytesTotal = 0;
    QString m_currentName;
    QString m_status;
};
