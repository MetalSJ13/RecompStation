#include "LibraryMetadataMatcher.h"

#include "ArtworkManager.h"
#include "../database/DatabaseManager.h"
#include "../metadata/MetadataManager.h"
#include "Logging.h"

#include <QRegularExpression>
#include <QString>
#include <QVariantMap>

namespace {
QString normalizedTitle(QString title)
{
    // Strip recognized region identifiers, but preserve revision tags.
    static const QRegularExpression regionTag(
        QStringLiteral(
            R"(\s*[\(\[]\s*(?:USA|US|U|Europe|EUR|EU|E|Japan|JPN|JP|J|World|W)\s*[\)\]])"),
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
        const QString localTitle =
            m_currentGame.value(QStringLiteral("title")).toString();
        const int expectedPlatformId = m_metadata->metadataPlatformId(
            m_currentGame.value(QStringLiteral("platform")).toString());
        const int matchIndex = bestMatchIndex(localTitle, expectedPlatformId, results);

        if (matchIndex < 0) {
            qCWarning(lcMetadata) << "No confident match for" << localTitle
                                  << "platform ID:" << expectedPlatformId
                                  << "candidate count:" << results.size();
            const bool ambiguous = matchIndex == -2;
            const int gameId = m_currentGame.value(QStringLiteral("id")).toInt();
            if (m_database) {
                m_database->markMetadataLookupAttempt(
                    gameId, ambiguous ? QStringLiteral("ambiguous")
                                      : QStringLiteral("no_match"));
            }
            finishCurrent(false, ambiguous
                ? QStringLiteral("Multiple equally strong metadata matches; left unassociated.")
                : QStringLiteral("No confident metadata match; left unassociated."));
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
            qCCritical(lcMetadata) << "Failed to save metadata association for game" << gameId;
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

        qCInfo(lcMetadata) << "Matched library game" << localTitle
                           << "to provider title:"
                           << match.value(QStringLiteral("title")).toString();
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
        qCWarning(lcMetadata) << "Metadata lookup failed for the current game:"
                               << m_currentGame.value(QStringLiteral("title")).toString()
                               << message;
        // A provider/network failure is not a definitive no-match. Leave
        // lookup-attempt records untouched so a later scan can retry.
        m_unresolvedCount += 1 + m_queue.size();
        m_completedCount += 1 + m_queue.size();
        m_queue.clear();
        m_currentGame.clear();
        emit progressChanged();
        setStatus(QStringLiteral("Metadata lookup stopped: %1").arg(message));
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

int LibraryMetadataMatcher::bestMatchIndex(const QString &localTitle,
                                           int platformId,
                                           const QVariantList &results)
{
    const QString wanted = normalizedTitle(localTitle);
    const QString trimmedTitle = localTitle.trimmed();
    if (wanted.isEmpty() || platformId <= 0)
        return -1;

    int bestIndex = -1;
    int bestTitleRank = 0;
    int bestArtworkPixels = -1;
    bool tied = false;

    for (int i = 0; i < results.size(); ++i) {
        const QVariantMap candidate = results.at(i).toMap();
        if (candidate.value(QStringLiteral("platformId")).toInt() != platformId)
            continue;

        const QString candidateTitle =
            candidate.value(QStringLiteral("title")).toString().trimmed();
        const QString normalizedCandidate = normalizedTitle(candidateTitle);
        int titleRank = 0;
        if (normalizedCandidate == wanted) {
            titleRank = 2;
        } else {
            const QString prefix = trimmedTitle;
            if (candidateTitle.size() > prefix.size()
                && candidateTitle.startsWith(prefix, Qt::CaseInsensitive)) {
                const QChar delimiter = candidateTitle.at(prefix.size());
                if (delimiter == QLatin1Char(':') || delimiter == QLatin1Char('-')
                    || delimiter == QChar(0x2013) || delimiter == QChar(0x2014)) {
                    titleRank = 1;
                }
            }
        }
        if (titleRank == 0)
            continue;

        const int artworkPixels =
            candidate.value(QStringLiteral("artworkPixelCount")).toInt();
        if (titleRank > bestTitleRank
            || (titleRank == bestTitleRank && artworkPixels > bestArtworkPixels)) {
            bestIndex = i;
            bestTitleRank = titleRank;
            bestArtworkPixels = artworkPixels;
            tied = false;
        } else if (titleRank == bestTitleRank
                   && artworkPixels == bestArtworkPixels) {
            tied = true;
        }
    }

    return bestIndex < 0 ? -1 : (tied ? -2 : bestIndex);
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


void LibraryMetadataMatcher::scrapeGame(int gameId)
{
    qCInfo(lcMetadata) << "Manual metadata retry requested for game ID:" << gameId;
    if (m_busy || !m_database || !m_metadata)
        return;

    if (gameId <= 0) {
        setStatus(QStringLiteral("A valid library game is required."));
        return;
    }
    if (!m_metadata->isConfigured()) {
        setStatus(QStringLiteral("Metadata is unavailable. Configure a TheGamesDB API key to enable matching."));
        return;
    }

    const QVariantList games = m_database->games();
    QVariantMap selected;
    for (const QVariant &entry : games) {
        const QVariantMap game = entry.toMap();
        if (game.value(QStringLiteral("id")).toInt() == gameId) {
            selected = game;
            break;
        }
    }

    if (selected.isEmpty()) {
        setStatus(QStringLiteral("The selected game is no longer in the library."));
        return;
    }

    m_queue.clear();
    m_queue.enqueue(QVariantMap{
        {QStringLiteral("id"), gameId},
        {QStringLiteral("title"), selected.value(QStringLiteral("title"))},
        {QStringLiteral("platform"), selected.value(QStringLiteral("platform"))},
        {QStringLiteral("hasMetadata"), !selected.value(QStringLiteral("metadataTitle")).toString().isEmpty()},
        {QStringLiteral("artworkUrl"), selected.value(QStringLiteral("metadataArtworkUrl"))}
    });

    m_completedCount = 0;
    m_matchedCount = 0;
    m_unresolvedCount = 0;
    m_totalCount = 1;
    emit progressChanged();

    setBusy(true);
    setStatus(QStringLiteral("Retrying metadata for %1...")
                  .arg(selected.value(QStringLiteral("title")).toString()));
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
    qCInfo(lcMetadata) << "Processing metadata for:"
                       << m_currentGame.value(QStringLiteral("title")).toString()
                       << "platform:" << m_currentGame.value(QStringLiteral("platform")).toString();

    // A game with saved metadata but missing local artwork should reuse its
    // persisted provider URL instead of repeating title matching.
    const bool hasMetadata =
        m_currentGame.value(QStringLiteral("hasMetadata")).toBool();
    const QString savedArtworkUrl =
        m_currentGame.value(QStringLiteral("artworkUrl")).toString().trimmed();
    const int localGameId =
        m_currentGame.value(QStringLiteral("id")).toInt();
    if (hasMetadata && !savedArtworkUrl.isEmpty() && m_artwork) {
        m_artwork->downloadArtwork(localGameId, savedArtworkUrl);
        finishCurrent(true,
            QStringLiteral("Saved metadata found; retrying its artwork download."));
        return;
    }

    const QString platform =
        m_currentGame.value(QStringLiteral("platform")).toString();
    const int platformId = m_metadata->metadataPlatformId(platform);
    if (platformId <= 0) {
        if (m_metadata->platformsLoading()) {
            setStatus(QStringLiteral("Waiting for the metadata platform catalog."));
            m_queue.prepend(m_currentGame);
            m_currentGame.clear();
            setBusy(false);
            return;
        }

        // The catalog has finished loading but has no mapping for this
        // platform. Count this game as unresolved and continue with the rest
        // of the queue rather than blocking every subsequent platform.
        qCWarning(lcMetadata) << "No metadata provider platform mapping for"
                              << platform << "- skipping this game for now.";
        finishCurrent(false,
            QStringLiteral("No provider platform mapping for %1; skipped")
                .arg(platform));
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
