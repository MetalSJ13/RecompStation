#pragma once

#include "MetadataProvider.h"

#include <QNetworkAccessManager>

class TheGamesDbClient : public MetadataProvider
{
    Q_OBJECT

public:
    explicit TheGamesDbClient(
        const QString &apiKey = qEnvironmentVariable("RECOMPSTATION_TGDB_API_KEY"),
        QObject *parent = nullptr);

    QString providerId() const override;
    bool isConfigured() const override;
    void searchGames(const QString &searchTerm,
                     int platformId = 0) override;

    static bool parseSearchResponse(const QByteArray &response,
                                    QList<GameMetadata> *results,
                                    QString *errorMessage = nullptr);

private:
    QString m_apiKey;
    QNetworkAccessManager m_network;
    int m_nextRequestId = 1;
};
