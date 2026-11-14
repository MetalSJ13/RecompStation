#pragma once

#include "GameMetadata.h"

#include <QObject>
#include <QVariantList>

class MetadataProvider;

class MetadataManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool configured READ isConfigured NOTIFY configurationChanged)
    Q_PROPERTY(bool searching READ isSearching NOTIFY searchingChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

public:
    explicit MetadataManager(MetadataProvider *provider, QObject *parent = nullptr);

    bool isConfigured() const;
    bool isSearching() const;
    QVariantList results() const;
    QString errorString() const;

    Q_INVOKABLE void searchGames(const QString &searchTerm, int platformId = 0);

signals:
    void configurationChanged();
    void searchingChanged();
    void resultsChanged();
    void errorChanged();
    void searchCompleted();
    void searchFailed(const QString &message);

private:
    void setSearching(bool searching);
    void setErrorString(const QString &message);

    MetadataProvider *m_provider = nullptr;
    QVariantList m_results;
    QString m_errorString;
    bool m_searching = false;
};
