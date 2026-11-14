#pragma once

#include "GameMetadata.h"\n#include "MetadataPlatformMapping.h"

#include <QObject>
#include <QVariantList>

class MetadataProvider;

class MetadataManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool configured READ isConfigured NOTIFY configurationChanged)
    Q_PROPERTY(bool searching READ isSearching NOTIFY searchingChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)\n    Q_PROPERTY(QVariantList platformOptions READ platformOptions NOTIFY platformOptionsChanged)\n    Q_PROPERTY(bool platformsLoading READ platformsLoading NOTIFY platformsLoadingChanged)

public:
    explicit MetadataManager(MetadataProvider *provider, QObject *parent = nullptr);

    bool isConfigured() const;
    bool isSearching() const;
    QVariantList results() const;
    QString errorString() const;\n    QVariantList platformOptions() const;\n    bool platformsLoading() const;\n    Q_INVOKABLE int metadataPlatformId(const QString &internalPlatform) const;

    Q_INVOKABLE void searchGames(const QString &searchTerm, int platformId = 0);

signals:
    void configurationChanged();
    void searchingChanged();
    void resultsChanged();
    void errorChanged();
    void searchCompleted();
    void searchFailed(const QString &message);\n    void platformOptionsChanged();\n    void platformsLoadingChanged();

private:
    void setSearching(bool searching);
    void setErrorString(const QString &message);

    MetadataProvider *m_provider = nullptr;
    QVariantList m_results;
    QString m_errorString;
    bool m_searching = false;\n    QList<TheGamesDbPlatform> m_platforms;\n    QVariantList m_platformOptions;\n    bool m_platformsLoading = true;
};
