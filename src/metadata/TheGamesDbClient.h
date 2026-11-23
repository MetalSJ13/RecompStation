#pragma once

#include "MetadataProvider.h"

#include <QNetworkAccessManager>

class TheGamesDbClient : public MetadataProvider
{
    Q_OBJECT

public:
    explicit TheGamesDbClient(
        const QString &apiKey = QString(),
        QObject *parent = nullptr);

    QString providerId() const override;
    bool isConfigured() const override;
    void setApiKey(const QString &apiKey);
    // TheGamesDB reports "remaining_monthly_allowance" (plus any
    // "extra_allowance") in every response. Returns -1 when absent.
    static int remainingAllowance(const QByteArray &response);
    void searchGames(const QString &searchTerm,
                     int platformId = 0) override;
    void fetchPlatforms() override;

    static bool parsePlatformResponse(const QByteArray &response,
                                      QList<TheGamesDbPlatform> *platforms,
                                      QString *errorMessage = nullptr);

    static bool parseSearchResponse(const QByteArray &response,
                                    QList<GameMetadata> *results,
                                    QString *errorMessage = nullptr);

private:
    QString m_apiKey;
    QNetworkAccessManager m_network;
    int m_nextRequestId = 1;
};
