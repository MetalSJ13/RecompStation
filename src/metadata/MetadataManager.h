#pragma once

#include "GameMetadata.h"
#include "MetadataPlatformMapping.h"

#include <QObject>
#include <QVariantList>

class MetadataProvider;
class TheGamesDbAllowance;

class MetadataManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool configured READ isConfigured NOTIFY configurationChanged)
    Q_PROPERTY(bool providerEnabled READ isProviderEnabled NOTIFY configurationChanged)
    Q_PROPERTY(bool searching READ isSearching NOTIFY searchingChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QVariantList platformOptions READ platformOptions NOTIFY platformOptionsChanged)
    Q_PROPERTY(bool platformsLoading READ platformsLoading NOTIFY platformsLoadingChanged)
    // "disabled", "unconfigured" (no key), "checking", "connected", "ready"
    // (platform list loaded from the saved copy; the key has not been used
    // yet this session) or "error".
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QString connectionError READ connectionError NOTIFY connectionStateChanged)
    // Requests left as last reported by TheGamesDB; -1 if unknown.
    Q_PROPERTY(int remainingAllowance READ remainingAllowance NOTIFY remainingAllowanceChanged)
    // Non-empty while automatic TheGamesDB lookups are paused to save requests.
    Q_PROPERTY(QString automaticPauseReason READ automaticPauseReason NOTIFY remainingAllowanceChanged)

public:
    explicit MetadataManager(MetadataProvider *provider, QObject *parent = nullptr,
                             bool providerEnabled = true);

    bool isConfigured() const;
    bool isProviderEnabled() const;
    void setProviderEnabled(bool enabled);
    bool isSearching() const;
    QVariantList results() const;
    QString errorString() const;
    QVariantList platformOptions() const;
    bool platformsLoading() const;
    Q_INVOKABLE int metadataPlatformId(const QString &internalPlatform) const;
    QString connectionState() const;
    QString connectionError() const;
    int remainingAllowance() const;
    // Shares the request allowance tracker with this manager. Without one,
    // automatic lookups are always allowed.
    void setAllowance(TheGamesDbAllowance *allowance);
    bool automaticLookupsAllowed() const;
    QString automaticPauseReason() const;
    // Re-reads the provider's configuration (for example after the API key
    // changed) and checks the connection by loading its platform list.
    Q_INVOKABLE void refreshProvider();

    Q_INVOKABLE void searchGames(const QString &searchTerm, int platformId = 0);

signals:
    void configurationChanged();
    void searchingChanged();
    void resultsChanged();
    void errorChanged();
    void searchCompleted();
    void searchFailed(const QString &message);
    void platformOptionsChanged();
    void platformsLoadingChanged();
    void connectionStateChanged();
    void remainingAllowanceChanged();

private:
    void setSearching(bool searching);
    void setErrorString(const QString &message);
    // allowCached: use the provider's saved platform list when it has one.
    void startPlatformFetch(bool allowCached);
    void setConnectionState(const QString &state, const QString &error = {});

    MetadataProvider *m_provider = nullptr;
    QVariantList m_results;
    QString m_errorString;
    bool m_searching = false;
    bool m_providerEnabled = true;
    QList<TheGamesDbPlatform> m_platforms;
    QVariantList m_platformOptions;
    bool m_platformsLoading = true;
    QString m_connectionState = QStringLiteral("checking");
    QString m_connectionError;
    int m_remainingAllowance = -1;
    TheGamesDbAllowance *m_allowance = nullptr;
    bool m_loadingFromCache = false;
};
