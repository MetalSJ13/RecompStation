#pragma once

#include "GameMetadata.h"
#include "MetadataPlatformMapping.h"

#include <QObject>
#include <QVariantList>

class MetadataProvider;

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

private:
    void setSearching(bool searching);
    void setErrorString(const QString &message);

    MetadataProvider *m_provider = nullptr;
    QVariantList m_results;
    QString m_errorString;
    bool m_searching = false;
    bool m_providerEnabled = true;
    QList<TheGamesDbPlatform> m_platforms;
    QVariantList m_platformOptions;
    bool m_platformsLoading = true;
};
