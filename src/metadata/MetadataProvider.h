#pragma once

#include "GameMetadata.h"
#include "MetadataPlatformMapping.h"

#include <QObject>
#include <QString>

class MetadataProvider : public QObject
{
    Q_OBJECT

public:
    explicit MetadataProvider(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~MetadataProvider() override = default;

    virtual QString providerId() const = 0;
    virtual bool isConfigured() const = 0;
    virtual void searchGames(const QString &searchTerm,
                             int platformId = 0) = 0;
    virtual void fetchPlatforms() = 0;
    // Emits platformsLoaded from a saved copy of the platform list and
    // returns true, or returns false when there is no recent copy.
    virtual bool loadCachedPlatforms() { return false; }

signals:
    void searchCompleted(int requestId, const QList<GameMetadata> &results);
    void requestFailed(int requestId, const QString &message);
    void platformsLoaded(const QList<TheGamesDbPlatform> &platforms);
    void platformsFailed(const QString &message);
    // Requests left in the provider's allowance, when a response reports it.
    void allowanceReported(int remaining);
};
