#pragma once

#include "MetadataProvider.h"

#include <QNetworkAccessManager>
#include <QPointer>

class TheGamesDbAllowance;

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
    bool loadCachedPlatforms() override;

    // Where the platform list is saved after a successful fetch. The list
    // rarely changes, so a copy younger than PlatformCacheDays is used at
    // startup instead of a request.
    static constexpr int PlatformCacheDays = 30;
    void setPlatformCachePath(const QString &path);
    // Receives the allowance every response reports, and 403 rejections.
    void setAllowance(TheGamesDbAllowance *allowance);

    static bool parsePlatformResponse(const QByteArray &response,
                                      QList<TheGamesDbPlatform> *platforms,
                                      QString *errorMessage = nullptr);

    static bool parseSearchResponse(const QByteArray &response,
                                    QList<GameMetadata> *results,
                                    QString *errorMessage = nullptr);

private:
    void recordAllowance(const QByteArray &body);
    void recordRejection();

    QString m_apiKey;
    QString m_platformCachePath;
    QPointer<TheGamesDbAllowance> m_allowance;
    QNetworkAccessManager m_network;
    int m_nextRequestId = 1;
};
