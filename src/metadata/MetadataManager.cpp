#include "MetadataManager.h"

#include "MetadataProvider.h"

#include <QPair>
#include <QVariantMap>

MetadataManager::MetadataManager(MetadataProvider *provider, QObject *parent)
    : QObject(parent),
      m_provider(provider)
{
    if (!m_provider)
        return;

    m_platformOptions.append(QVariantMap{
        {QStringLiteral("internalName"), QStringLiteral("All Platforms")},
        {QStringLiteral("name"), QStringLiteral("All Platforms")},
        {QStringLiteral("id"), 0}});

    connect(m_provider, &MetadataProvider::platformsLoaded,
            this, [this](const QList<TheGamesDbPlatform> &platforms) {
        m_platforms = platforms;
        m_platformOptions.clear();
        m_platformOptions.append(QVariantMap{
            {QStringLiteral("internalName"), QStringLiteral("All Platforms")},
            {QStringLiteral("name"), QStringLiteral("All Platforms")},
            {QStringLiteral("id"), 0}});
        const QList<QPair<QString, QString>> supported = {
            {QStringLiteral("N64"), QStringLiteral("Nintendo 64")},
            {QStringLiteral("PS1"), QStringLiteral("PlayStation")},
            {QStringLiteral("GameCube"), QStringLiteral("Nintendo GameCube")},
            {QStringLiteral("Wii"), QStringLiteral("Nintendo Wii")}
        };
        for (const auto &entry : supported) {
            const int id = MetadataPlatformMapping::platformIdForInternalName(entry.first, m_platforms);
            if (id <= 0)
                continue;
            m_platformOptions.append(QVariantMap{
                {QStringLiteral("internalName"), entry.first},
                {QStringLiteral("name"), entry.second},
                {QStringLiteral("id"), id}});
        }
        m_platformsLoading = false;
        emit platformOptionsChanged();
        emit platformsLoadingChanged();
    });
    connect(m_provider, &MetadataProvider::platformsFailed,
            this, [this](const QString &message) {
        m_platformsLoading = false;
        setErrorString(message);
        emit platformsLoadingChanged();
    });
    m_provider->fetchPlatforms();

    connect(m_provider, &MetadataProvider::searchCompleted,
            this, [this](int, const QList<GameMetadata> &games) {
        QVariantList results;
        results.reserve(games.size());

        for (const GameMetadata &game : games) {
            QVariantMap item;
            item.insert(QStringLiteral("provider"), game.provider);
            item.insert(QStringLiteral("providerGameId"), game.providerGameId);
            item.insert(QStringLiteral("title"), game.title);
            item.insert(QStringLiteral("overview"), game.overview);
            item.insert(QStringLiteral("releaseDate"),
                         game.releaseDate.isValid()
                             ? game.releaseDate.toString(Qt::ISODate)
                             : QString());
            item.insert(QStringLiteral("platformId"), game.platformId);
            item.insert(QStringLiteral("rating"), game.rating);
            item.insert(QStringLiteral("artworkUrl"), game.artworkUrl);
            results.append(item);
        }

        m_results = results;
        emit resultsChanged();
        setSearching(false);
        setErrorString(QString());
        emit searchCompleted();
    });

    connect(m_provider, &MetadataProvider::requestFailed,
            this, [this](int, const QString &message) {
        setSearching(false);
        setErrorString(message);
        emit searchFailed(message);
    });
}

bool MetadataManager::isConfigured() const
{
    return m_provider && m_provider->isConfigured();
}

bool MetadataManager::isSearching() const
{
    return m_searching;
}

QVariantList MetadataManager::results() const
{
    return m_results;
}

QString MetadataManager::errorString() const
{
    return m_errorString;
}

QVariantList MetadataManager::platformOptions() const
{
    return m_platformOptions;
}

bool MetadataManager::platformsLoading() const
{
    return m_platformsLoading;
}

int MetadataManager::metadataPlatformId(const QString &internalPlatform) const
{
    return MetadataPlatformMapping::platformIdForInternalName(internalPlatform, m_platforms);
}

void MetadataManager::searchGames(const QString &searchTerm, int platformId)
{
    if (!m_provider) {
        setErrorString(QStringLiteral("No metadata provider is available."));
        emit searchFailed(m_errorString);
        return;
    }

    m_results.clear();
    emit resultsChanged();
    setErrorString(QString());
    setSearching(true);
    m_provider->searchGames(searchTerm, platformId);
}

void MetadataManager::setSearching(bool searching)
{
    if (m_searching == searching)
        return;
    m_searching = searching;
    emit searchingChanged();
}

void MetadataManager::setErrorString(const QString &message)
{
    if (m_errorString == message)
        return;
    m_errorString = message;
    emit errorChanged();
}
