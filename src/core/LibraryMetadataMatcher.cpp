#include "LibraryMetadataMatcher.h"

#include "ArtworkManager.h"
#include "../database/DatabaseManager.h"
#include "../metadata/MetadataManager.h"

#include <QRegularExpression>
#include <QString>
#include <QVariantMap>

namespace {
QString normalizedTitle(QString title)
{
    static const QRegularExpression regionTag(
        QStringLiteral(R"(\s*\((?:USA|US|Europe|EUR|Japan|JPN|World|Rev(?:ision)? ?[A-Z0-9]*)\)|\s*\[(?:!|b)\])"),
        QRegularExpression::CaseInsensitiveOption);
    title.remove(regionTag);
    title = title.normalized(QString::NormalizationForm_D);
    QString result;
    result.reserve(title.size());

    for (const QChar character : title) {
        if (character.category() == QChar::Mark_NonSpacing
            || character.category() == QChar::Mark_SpacingCombining
            || character.category() == QChar::Mark_Enclosing) {
            continue;
        }
        if (character.isLetterOrNumber())
            result.append(character.toCaseFolded());
        else
            result.append(QLatin1Char(' '));
    }
    return result.simplified();
}
}

LibraryMetadataMatcher::LibraryMetadataMatcher(DatabaseManager *database,
                                               MetadataManager *metadata,
                                               ArtworkManager *artwork,
                                               QObject *parent)
    : QObject(parent),
      m_database(database),
      m_metadata(metadata),
      m_artwork(artwork)
{
    if (!m_metadata)
        return;

    if (m_artwork) {
        connect(m_artwork, &ArtworkManager::artworkFailed, this,
                [this](int gameId, const QString &message) {
            setStatus(QStringLiteral("Artwork for game %1 could not be downloaded: %2")
                          .arg(gameId).arg(message));
        });
    }

    if (m_artwork) {
        connect(m_artwork, &ArtworkManager::artworkFailed, this,
                [this](int gameId, const QString &message) {
            setStatus(QStringLiteral("Artwork for game %1 could not be downloaded: %2")
                          .arg(gameId).arg(message));
        });
    }

    connect(m_metadata, &MetadataManager::platformOptionsChanged,
            this, [this]() {
        if (!m_busy && !m_queue.isEmpty()) {
            setBusy(true);
            processNext();
        }
    });

    connect(m_metadata, &MetadataManager::searchCompleted,
            this, [this]() {
        if (!m_busy || m_currentGame.isEmpty())
            return;

        const QVariantList results = m_metadata->results();
        const int matchIndex = exactMatchIndex(
            m_currentGame.value(QStringLiteral("title")).toString(), results);

        if (matchIndex < 0) {
            const bool ambiguous = matchIndex == -2;
            const int gameId = m_currentGame.value(QStringLiteral("id")).toInt();
            if (m_database) {
                m_database->markMetadataLookupAttempt(
                    gameId, ambiguous ? QStringLiteral("ambiguous")
                                      : QStringLiteral("no_match"));
            }
            finishCurrent(false, ambiguous
                ? QStringLiteral("Multiple exact metadata matches; left unassociated.")
                : QStringLiteral("No exact metadata match; left unassociated."));
            return;
        }

        const QVariantMap match = results.at(matchIndex).toMap();
        const int gameId = m_currentGame.value(QStringLiteral("id")).toInt();
        const bool saved = m_database && m_database->associateMetadata(
            gameId,
            match.value(QStringLiteral("provider")).toString(),
            match.value(QStringLiteral("providerGameId")).toInt(),
            match.value(QStringLiteral("title")).toString(),
            match.value(QStringLiteral("overview")).toString(),
            match.value(QStringLiteral("releaseDate")).toString(),
            match.value(QStringLiteral("platformId")).toInt(),
            match.value(QStringLiteral("rating")).toString(),
            match.value(QStringLiteral("artworkUrl")).toString());

        if (!saved) {
            setStatus(QStringLiteral("Metadata could not be saved: %1")
                          .arg(m_database ? m_database->lastError()
                                          : QStringLiteral("Database unavailable.")));
            setBusy(false);
            m_queue.clear();
            emit scrapeFinished(m_matchedCount, m_unresolvedCount);
            return;
        }

        if (m_database)
            m_database->markMetadataLookupAttempt(gameId, QStringLiteral("matched"));

        const QString artworkUrl =
            match.value(QStringLiteral("artworkUrl")).toString();
        if (m_artwork && !artworkUrl.isEmpty()) {
            m_artwork->downloadArtwork(gameId, artworkUrl);
            finishCurrent(true, QStringLiteral("Metadata matched; artwork download started."));
        } else {
            finishCurrent(true, QStringLiteral("Metadata matched, but the provider returned no cover artwork."));
        }
    });

    connect(m_metadata, &MetadataManager::searchFailed,
            this, [this](const QString &message) {
        if (!m_busy)
            return;
        setStatus(QStringLiteral("Metadata lookup stopped: %1").arg(message));
        m_queue.clear();
        m_currentGame.clear();
        setBusy(false);
        emit scrapeFinished(m_matchedCount, m_unresolvedCount);
    });
}

bool LibraryMetadataMatcher::isBusy() const { return m_busy; }
int LibraryMetadataMatcher::completedCount() const { return m_completedCount; }
int LibraryMetadataMatcher::totalCount() const { return m_totalCount; }
QString LibraryMetadataMatcher::status() const { return m_status; }

int LibraryMetadataMatcher::exactMatchIndex(const QString &localTitle,
                                            const QVariantList &results)
{
    const QString wanted = normalizedTitle(localTitle);
    if (wanted.isEmpty())
        return -1;

    int match = -1;
    for (int i = 0; i < results.size(); ++i) {
        const QString candidate = normalizedTitle(
            results.at(i).toMap().value(QStringLiteral("title")).toString());
        if (candidate != wanted)
            continue;
        if (match >= 0)
            return -2;
        match = i;
    }
    return match;
}

void LibraryMetadataMatcher::scrapeLibrary(bool includePreviouslyChecked)
{
    if (m_busy || !m_database || !m_metadata)
        return;

    if (!m_metadata->isConfigured()) {
        setStatus(QStringLiteral("Metadata is unavailable. Configure a TheGamesDB API key to enable automatic matching."));
        return;
    }

    const QVariantList games = m_database->gamesNeedingMetadata(includePreviouslyChecked);
    m_queue.clear();
    for (const QVariant &entry : games)
        m_queue.enqueue(entry.toMap());

    m_completedCount = 0;
    m_matchedCount = 0;
    m_unresolvedCount = 0;
    m_totalCount = m_queue.size();
    emit progressChanged();

    if (m_queue.isEmpty()) {
        setStatus(QStringLiteral("All games already have metadata or have been checked."));
        emit scrapeFinished(0, 0);
        return;
    }

    setBusy(true);
    setStatus(QStringLiteral("Matching metadata for %1 game(s)...").arg(m_totalCount));
    processNext();
}

void LibraryMetadataMatcher::processNext()
{
    if (!m_busy)
        return;

    if (m_queue.isEmpty()) {
        setBusy(false);
        setStatus(QStringLiteral("Metadata complete: %1 matched, %2 left unresolved.")
                      .arg(m_matchedCount).arg(m_unresolvedCount));
        emit scrapeFinished(m_matchedCount, m_unresolvedCount);
        return;
    }

    m_currentGame = m_queue.dequeue();
    const QString platform =
        m_currentGame.value(QStringLiteral("platform")).toString();
    const int platformId = m_metadata->metadataPlatformId(platform);
    if (platformId <= 0) {
        setStatus(QStringLiteral("Waiting for the metadata platform catalog."));
        m_queue.prepend(m_currentGame);
        m_currentGame.clear();
        setBusy(false);
        return;
    }

    setStatus(QStringLiteral("Checking %1...")
                  .arg(m_currentGame.value(QStringLiteral("title")).toString()));
    m_metadata->searchGames(
        m_currentGame.value(QStringLiteral("title")).toString(), platformId);
}

void LibraryMetadataMatcher::finishCurrent(bool matched, const QString &reason)
{
    if (matched)
        ++m_matchedCount;
    else
        ++m_unresolvedCount;

    ++m_completedCount;
    m_currentGame.clear();
    emit progressChanged();
    if (!reason.isEmpty())
        setStatus(QStringLiteral("%1 (%2/%3)")
                      .arg(reason).arg(m_completedCount).arg(m_totalCount));
    processNext();
}

void LibraryMetadataMatcher::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void LibraryMetadataMatcher::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}
