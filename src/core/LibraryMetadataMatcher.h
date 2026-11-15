#pragma once

#include <QObject>
#include <QQueue>
#include <QVariantList>
#include <QVariantMap>

class ArtworkManager;
class DatabaseManager;
class MetadataManager;

class LibraryMetadataMatcher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(int completedCount READ completedCount NOTIFY progressChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit LibraryMetadataMatcher(DatabaseManager *database,
                                    MetadataManager *metadata,
                                    ArtworkManager *artwork,
                                    QObject *parent = nullptr);

    bool isBusy() const;
    int completedCount() const;
    int totalCount() const;
    QString status() const;

    Q_INVOKABLE void scrapeLibrary(bool includePreviouslyChecked = false);

    static int exactMatchIndex(const QString &localTitle,
                               const QVariantList &results);
    static int bestMatchIndex(const QString &localTitle, int platformId,
                              const QVariantList &results);

signals:
    void busyChanged();
    void progressChanged();
    void statusChanged();
    void scrapeFinished(int matched, int unresolved);

private:
    void processNext();
    void finishCurrent(bool matched, const QString &reason = {});
    void setBusy(bool busy);
    void setStatus(const QString &status);

    DatabaseManager *m_database = nullptr;
    MetadataManager *m_metadata = nullptr;
    ArtworkManager *m_artwork = nullptr;
    QQueue<QVariantMap> m_queue;
    QVariantMap m_currentGame;
    int m_completedCount = 0;
    int m_totalCount = 0;
    int m_matchedCount = 0;
    int m_unresolvedCount = 0;
    bool m_busy = false;
    QString m_status;
};
