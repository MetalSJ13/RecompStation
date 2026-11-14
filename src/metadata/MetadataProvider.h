#pragma once

#include "GameMetadata.h"\n#include "MetadataPlatformMapping.h"

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

signals:
    void searchCompleted(int requestId, const QList<GameMetadata> &results);
    void requestFailed(int requestId, const QString &message);\n    void platformsLoaded(const QList<TheGamesDbPlatform> &platforms);\n    void platformsFailed(const QString &message);
};
