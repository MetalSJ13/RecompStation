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

signals:
    void searchCompleted(int requestId, const QList<GameMetadata> &results);
    void requestFailed(int requestId, const QString &message);
    void platformsLoaded(const QList<TheGamesDbPlatform> &platforms);
    void platformsFailed(const QString &message);
};
