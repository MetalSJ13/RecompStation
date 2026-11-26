#include "LibraryMetadataMatcher.h"

#include <QTimer>

#include "ArtworkManager.h"
#include "../database/DatabaseManager.h"
#include "../metadata/MetadataManager.h"
#include "../metadata/LaunchBoxCatalogManager.h"
#include "../metadata/LaunchBoxCatalogDatabase.h"
#include "../metadata/LaunchBoxCatalogMatcher.h"
#include "SettingsManager.h"
#include "Logging.h"

#include <QRegularExpression>
#include <QFileInfo>
#include <algorithm>
#include <QString>
#include <QVariantMap>

namespace {
// "1 game", "3 games".
QString gamesCount(int count)
{
    return count == 1 ? QStringLiteral("1 game") : QStringLiteral("%1 games").arg(count);
}
}


namespace {
QString normalizedTitle(QString title)
{
    // ROM databases commonly put the leading article at the end of a title,
    // e.g. "Legend of Zelda, The - Ocarina of Time". Move it to the front
    // before producing the provider query and comparison key.
    static const QRegularExpression trailingArticle(
        QStringLiteral(R"(^\s*(.+?),\s*The\s*[-:–—]\s*(.+?)\s*$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch articleMatch = trailingArticle.match(title);
    if (articleMatch.hasMatch()) {
        title = QStringLiteral("The %1 %2")
                    .arg(articleMatch.captured(1), articleMatch.captured(2));
    }

    // Strip recognized region identifiers, but preserve revision tags.
    static const QRegularExpression regionTag(
        QStringLiteral(
            // One or more regions: "(USA)", "(Japan, USA)", "(USA, Europe)".
            R"(\s*[\(\[]\s*(?:USA|US|U|Europe|EUR|EU|E|Japan|JPN|JP|J|World|W|Asia|Australia|Brazil|Canada|China|France|Germany|Hong Kong|Italy|Korea|Netherlands|Scandinavia|Spain|Sweden|Taiwan|UK|Latin America)(?:\s*[,;+]\s*(?:USA|US|U|Europe|EUR|EU|E|Japan|JPN|JP|J|World|W|Asia|Australia|Brazil|Canada|China|France|Germany|Hong Kong|Italy|Korea|Netherlands|Scandinavia|Spain|Sweden|Taiwan|UK|Latin America))*\s*[\)\]])"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression languageTag(
        QStringLiteral(R"(\s*[\(\[]\s*(?:EN|ENG|FR|FRE|ES|SPA|DE|GER|IT|ITA|NL|DUT|PT|POR|SV|SWE|NO|NOR|DA|DAN|FI|FIN|PL|POL|RU|RUS|JA|JPN|ZH|CHI|KO|KOR|CS|CZE|HU|HUN|TR|TUR|AR|HE|TH|EL|GRE|RO|RON|BG|BUL|HR|CRO|SK|SL|UK|UKR|VI|ID|MS|CA|EU|GL)(?:\s*[,;+]\s*(?:EN|ENG|FR|FRE|ES|SPA|DE|GER|IT|ITA|NL|DUT|PT|POR|SV|SWE|NO|NOR|DA|DAN|FI|FIN|PL|POL|RU|RUS|JA|JPN|ZH|CHI|KO|KOR|CS|CZE|HU|HUN|TR|TUR|AR|HE|TH|EL|GRE|RO|RON|BG|BUL|HR|CRO|SK|SL|UK|UKR|VI|ID|MS|CA|EU|GL))*\s*[\)\]])"),
        QRegularExpression::CaseInsensitiveOption);
    title.remove(regionTag);
    title.remove(languageTag);
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

QString normalizedTitleWithoutRevision(QString title)
{
    static const QRegularExpression revisionTag(
        QStringLiteral(R"(\s*[\(\[]\s*(?:Rev(?:ision)?|v)\s*[A-Z0-9.]+\s*[\)\]])"),
        QRegularExpression::CaseInsensitiveOption);
    title.remove(revisionTag);
    return normalizedTitle(title);
}
}

LibraryMetadataMatcher::LibraryMetadataMatcher(DatabaseManager *database,
                                               MetadataManager *metadata,
                                               ArtworkManager *artwork,
                                               LaunchBoxCatalogManager *launchBoxCatalog,
                                               SettingsManager *settings,
                                               QObject *parent)
    : QObject(parent),
      m_database(database),
      m_metadata(metadata),
      m_artwork(artwork),
      m_launchBoxCatalog(launchBoxCatalog),
      m_settings(settings)
{
    // A catalog update replaces the file: let go of it first.
    if (m_launchBoxCatalog) {
        connect(m_launchBoxCatalog, &LaunchBoxCatalogManager::busyChanged,
                this, [this]() { m_catalog.reset(); });
    }
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

        // LaunchBox is the second-chance provider whenever TheGamesDB
        // completes successfully but cannot produce a confident association.
        // This includes empty, unrelated, and ambiguous result sets. Provider
        // failures are handled separately by searchFailed and never fall back.
        if (matchIndex < 0) {
            const QString reason = results.isEmpty()
                ? QStringLiteral("TheGamesDB returned no results.")
                : (matchIndex == -2
                    ? QStringLiteral("TheGamesDB results were ambiguous.")
                    : QStringLiteral("TheGamesDB returned no confident match."));
            if (tryLaunchBoxFallback(reason))
                return;
        }

        if (matchIndex < 0) {
            qCWarning(lcMetadata) << "No confident match for" << localTitle
                                  << "platform ID:" << expectedPlatformId
                                  << "candidate count:" << results.size();
            // Keep enough provider-side detail in the runtime log to
            // distinguish a title-normalization issue from genuinely
            // unrelated or cross-platform search results.
            for (const QVariant &entry : results) {
                const QVariantMap candidate = entry.toMap();
                qCDebug(lcMetadata) << "Metadata candidate:"
                                   << candidate.value(QStringLiteral("title")).toString()
                                   << "provider ID:"
                                   << candidate.value(QStringLiteral("providerGameId")).toInt()
                                   << "platform ID:"
                                   << candidate.value(QStringLiteral("platformId")).toInt()
                                   << "release date:"
                                   << candidate.value(QStringLiteral("releaseDate")).toString()
                                   << "artwork pixels:"
                                   << candidate.value(QStringLiteral("artworkPixelCount")).toInt()
                                   << "has overview:"
                                   << !candidate.value(QStringLiteral("overview")).toString().trimmed().isEmpty()
                                   << "has artwork URL:"
                                   << !candidate.value(QStringLiteral("artworkUrl")).toString().trimmed().isEmpty();
            }
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
        const bool downloadArtwork = m_forceArtworkDownload
            || (m_settings && m_settings->automaticArtworkDownloadsEnabled());
        if (downloadArtwork && m_artwork && !artworkUrl.isEmpty()) {
            m_artwork->downloadArtwork(gameId, artworkUrl);
            finishCurrent(true, QStringLiteral("Metadata matched; artwork download started."));
        } else if (!downloadArtwork && !artworkUrl.isEmpty()) {
            finishCurrent(true, QStringLiteral("Metadata matched; automatic artwork downloads are disabled."));
        } else {
            finishCurrent(true, QStringLiteral("Metadata matched, but the provider returned no cover artwork."));
        }
    });

    connect(m_metadata, &MetadataManager::searchFailed,
            this, [this](const QString &message) {
        if (!m_busy || m_currentGame.isEmpty())
            return;
        qCWarning(lcMetadata) << "Metadata lookup failed for the current game:"
                               << m_currentGame.value(QStringLiteral("title")).toString()
                               << message;
        // A provider or network failure (no key, rate limit, offline) is not
        // a definitive no-match. Stop asking TheGamesDB for the rest of this
        // run, but keep matching with LaunchBox. Games nobody could match are
        // recorded as "provider_error", which automatic scans retry after an
        // hour (DatabaseManager::gamesNeedingMetadata).
        m_theGamesDbUnavailable = true;
        m_theGamesDbFailure = message;
        if (!m_currentGame.value(QStringLiteral("launchBoxTried")).toBool()
                && launchBoxAvailable()) {
            tryLaunchBoxFallback(QStringLiteral("TheGamesDB request failed."),
                                 QStringLiteral("provider_error"));
            return;
        }
        markCurrent(QStringLiteral("provider_error"));
        finishCurrent(false, QStringLiteral("TheGamesDB unavailable: %1").arg(message));
    });
}

LibraryMetadataMatcher::~LibraryMetadataMatcher() = default;

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
            titleRank = 3;
        } else if (normalizedTitleWithoutRevision(candidateTitle)
                   == normalizedTitleWithoutRevision(localTitle)) {
            // A unique canonical provider title may omit a ROM's revision
            // suffix. Keep exact revision matches higher priority.
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

void LibraryMetadataMatcher::scrapeLibrary(bool includePreviouslyChecked,
                                            bool forceArtworkDownload)
{
    qCInfo(lcMetadata) << "Library metadata scan requested. Include previously checked:"
                       << includePreviouslyChecked
                       << "busy:" << m_busy
                       << "database available:" << (m_database != nullptr)
                       << "provider available:" << (m_metadata != nullptr);
    if (!m_database || !m_metadata) {
        qCInfo(lcMetadata) << "Library metadata scan skipped because the matcher is unavailable.";
        return;
    }
    if (m_busy) {
        // A scan finished while matching was still running (for example,
        // files copied in several batches). Run again once this pass ends so
        // the newly found games are not left until the next manual refresh.
        m_rescanPending = true;
        m_rescanIncludesChecked = m_rescanIncludesChecked || includePreviouslyChecked;
        qCInfo(lcMetadata) << "Library metadata scan queued until the current pass finishes.";
        return;
    }

    m_theGamesDbUnavailable = false;
    m_theGamesDbFailure.clear();
    m_singleGameRun = false;
    const bool primaryAvailable = m_metadata->isConfigured();
    if (!primaryAvailable && !launchBoxAvailable()) {
        qCWarning(lcMetadata) << "Library metadata scan skipped: no metadata provider is available.";
        setStatus(QStringLiteral("No source for game info is set up. Turn on LaunchBox or add a TheGamesDB key in Metadata."));
        return;
    }

    m_forceArtworkDownload = forceArtworkDownload;
    const QVariantList games = m_database->gamesNeedingMetadata(includePreviouslyChecked);
    qCInfo(lcMetadata) << "Library metadata scan queued" << games.size()
                       << "game(s) for matching.";
    m_queue.clear();
    for (const QVariant &entry : games)
        m_queue.enqueue(entry.toMap());

    m_completedCount = 0;
    m_matchedCount = 0;
    m_unresolvedCount = 0;
    m_totalCount = m_queue.size();
    emit progressChanged();

    if (m_queue.isEmpty()) {
        setStatus(QStringLiteral("Every game has been looked up."));
        emit scrapeFinished(0, 0);
        runPendingRescan();
        return;
    }

    setBusy(true);
    setStatus(QStringLiteral("Looking up game info for %1…").arg(gamesCount(m_totalCount)));
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
    m_theGamesDbUnavailable = false;
    m_theGamesDbFailure.clear();
    // "Find info" on one game is the user's own request: it may use the
    // requests kept in reserve, and shows whether TheGamesDB works again.
    m_singleGameRun = true;
    const bool primaryAvailable = m_metadata->isConfigured();
    if (!primaryAvailable && !launchBoxAvailable()) {
        setStatus(QStringLiteral("No source for game info is set up. Turn on LaunchBox or add a TheGamesDB key in Metadata."));
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

    m_forceArtworkDownload = true;
    m_queue.clear();
    m_queue.enqueue(QVariantMap{
        {QStringLiteral("id"), gameId},
        {QStringLiteral("title"), selected.value(QStringLiteral("title"))},
        {QStringLiteral("platform"), selected.value(QStringLiteral("platform"))},
        {QStringLiteral("hasMetadata"), !selected.value(QStringLiteral("metadataTitle")).toString().isEmpty()},
        {QStringLiteral("artworkUrl"), selected.value(QStringLiteral("metadataArtworkUrl"))},
        {QStringLiteral("path"), selected.value(QStringLiteral("path"))},
        {QStringLiteral("metadataProvider"), selected.value(QStringLiteral("metadataProvider"))}
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

LibraryMetadataMatcher::LaunchBoxOutcome
LibraryMetadataMatcher::attemptLaunchBoxMatch(QString *message)
{
    m_currentGame.insert(QStringLiteral("launchBoxTried"), true);
    const int gameId = m_currentGame.value(QStringLiteral("id")).toInt();
    const QString title = m_currentGame.value(QStringLiteral("title")).toString();
    const QString platform = m_currentGame.value(QStringLiteral("platform")).toString();
    if (m_currentGame.value(QStringLiteral("metadataProvider")).toString()
            == QStringLiteral("launchbox")) {
        *message = QStringLiteral("Existing LaunchBox metadata retained.");
        return LaunchBoxOutcome::Matched;
    }
    if (!launchBoxAvailable()) {
        m_catalog.reset();   // let an update replace the file
        *message = QStringLiteral("LaunchBox is disabled, not installed, or updating.");
        return LaunchBoxOutcome::Unavailable;
    }

    if (!m_catalog || !m_catalog->isOpen()) {
        m_catalog = std::make_unique<LaunchBoxCatalogDatabase>();
        if (!m_catalog->openForReading(m_launchBoxCatalog->catalogPath())) {
            qCWarning(lcMetadata) << "Could not open LaunchBox catalog:" << m_catalog->lastError();
            m_catalog.reset();
            *message = QStringLiteral("LaunchBox catalog could not be opened.");
            return LaunchBoxOutcome::Failed;
        }
    }
    LaunchBoxCatalogDatabase &catalog = *m_catalog;

    const QString localPath = m_currentGame.value(QStringLiteral("path")).toString();
    const QString fileName = localPath.isEmpty() ? title : QFileInfo(localPath).fileName();
    const auto associations = catalog.findFileAssociations(platform, fileName);
    QList<LaunchBoxGameRecord> candidates;
    if (!associations.isEmpty()) {
        for (const LaunchBoxFileAssociation &association : associations) {
            const auto records = catalog.findGamesByRevisionInsensitiveTitle(
                platform, association.gameName);
            for (const LaunchBoxGameRecord &record : records) {
                const bool duplicate = std::any_of(
                    candidates.cbegin(), candidates.cend(),
                    [&record](const LaunchBoxGameRecord &existing) {
                        return existing.databaseId == record.databaseId;
                    });
                if (!duplicate)
                    candidates.append(record);
            }
        }
    } else {
        // Query with the full revision-aware key first and its revisionless
        // counterpart as a secondary candidate source. The matcher still
        // prefers an exact revision and rejects multiple revisionless results.
        candidates = catalog.findGamesByRevisionInsensitiveTitle(platform, title);
    }

    const LaunchBoxMatchResult result = !associations.isEmpty()
        ? LaunchBoxCatalogMatcher::matchByFileAssociation(
              platform, fileName, associations, candidates)
        : LaunchBoxCatalogMatcher::matchByTitle(platform, title, candidates);

    if (!result.isMatch()) {
        qCInfo(lcMetadata) << "LaunchBox did not produce a unique match for"
                           << title << "reason:" << result.reason;
        // Which entries competed, so a wrong or missing match can be traced.
        for (const LaunchBoxGameRecord &candidate : std::as_const(candidates)) {
            qCInfo(lcMetadata).noquote()
                << "  candidate" << candidate.databaseId << candidate.name
                << "·" << candidate.platform
                << "· released" << (candidate.releaseDate.isValid()
                                        ? candidate.releaseDate.toString(Qt::ISODate)
                                        : QString::number(candidate.releaseYear))
                << "·" << (candidate.releaseType.isEmpty() ? QStringLiteral("no release type")
                                                            : candidate.releaseType)
                << "·" << candidate.communityRatingCount.toInt() << "ratings";
        }
        const bool ambiguous = result.status == LaunchBoxMatchResult::Status::Ambiguous;
        *message = ambiguous
            ? QStringLiteral("LaunchBox found ambiguous candidates.")
            : QStringLiteral("No confident LaunchBox match.");
        return ambiguous ? LaunchBoxOutcome::Ambiguous : LaunchBoxOutcome::NoMatch;
    }

    const LaunchBoxGameRecord &match = candidates.at(result.index);
    QString releaseDate;
    if (match.releaseDate.isValid())
        releaseDate = match.releaseDate.toString(Qt::ISODate);
    else if (match.releaseYear > 0)
        releaseDate = QString::number(match.releaseYear);

    const bool saved = m_database && m_database->associateMetadataIfMissing(
        gameId, QStringLiteral("launchbox"), match.databaseId, match.name,
        match.overview, releaseDate, 0, match.communityRating, QString());
    if (!saved) {
        qCWarning(lcMetadata) << "LaunchBox association was not saved for"
                              << title << (m_database ? m_database->lastError() : QString());
        *message = QStringLiteral("LaunchBox metadata could not be saved.");
        return LaunchBoxOutcome::Failed;
    }
    qCInfo(lcMetadata) << "LaunchBox matched" << title << "to" << match.name;
    *message = QStringLiteral("Matched with LaunchBox.");
    return LaunchBoxOutcome::Matched;
}

bool LibraryMetadataMatcher::tryLaunchBoxFallback(const QString &reason,
                                                  const QString &unmatchedStatus)
{
    if (m_currentGame.value(QStringLiteral("launchBoxTried")).toBool()) {
        // LaunchBox was asked first and had nothing; TheGamesDB had nothing either.
        markCurrent(unmatchedStatus);
        finishCurrent(false, QStringLiteral("No confident match from LaunchBox or TheGamesDB."));
        return true;
    }
    QString message;
    const LaunchBoxOutcome outcome = attemptLaunchBoxMatch(&message);
    qCInfo(lcMetadata) << "LaunchBox fallback after:" << reason << "-" << message;
    if (outcome == LaunchBoxOutcome::Matched) {
        markCurrent(QStringLiteral("matched"));
        finishCurrent(true, message);
        return true;
    }
    markCurrent(outcome == LaunchBoxOutcome::Ambiguous && unmatchedStatus == QStringLiteral("no_match")
        ? QStringLiteral("ambiguous") : unmatchedStatus);
    finishCurrent(false, message);
    return true;
}

bool LibraryMetadataMatcher::launchBoxAvailable() const
{
    return m_settings && m_settings->launchBoxEnabled()
        && m_launchBoxCatalog && m_launchBoxCatalog->catalogAvailable()
        && !m_launchBoxCatalog->busy();
}

void LibraryMetadataMatcher::markCurrent(const QString &status)
{
    const int gameId = m_currentGame.value(QStringLiteral("id")).toInt();
    if (m_database && gameId > 0 && !m_database->markMetadataLookupAttempt(gameId, status))
        qCWarning(lcMetadata) << "Could not record metadata lookup status" << status
                              << "for game" << gameId << m_database->lastError();
}

void LibraryMetadataMatcher::runPendingRescan()
{
    if (!m_rescanPending)
        return;
    const bool includeChecked = m_rescanIncludesChecked;
    m_rescanPending = false;
    m_rescanIncludesChecked = false;
    QTimer::singleShot(0, this, [this, includeChecked]() {
        scrapeLibrary(includeChecked);
    });
}

void LibraryMetadataMatcher::processNext()
{
    if (!m_busy)
        return;

    if (m_queue.isEmpty()) {
        m_catalog.reset();
        setBusy(false);
        m_forceArtworkDownload = false;
        setStatus(m_theGamesDbUnavailable
            ? QStringLiteral("Game info found for %1; %2 without a match. TheGamesDB couldn't be reached, so it's tried again later.")
                  .arg(gamesCount(m_matchedCount)).arg(m_unresolvedCount)
            : QStringLiteral("Game info found for %1; %2 without a match.")
                  .arg(gamesCount(m_matchedCount)).arg(m_unresolvedCount));
        emit scrapeFinished(m_matchedCount, m_unresolvedCount);
        runPendingRescan();
        return;
    }

    m_currentGame = m_queue.dequeue();
    qCDebug(lcMetadata) << "Processing metadata for:"
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
    const bool downloadArtwork = m_forceArtworkDownload
        || (m_settings && m_settings->automaticArtworkDownloadsEnabled());
    if (hasMetadata && !savedArtworkUrl.isEmpty() && m_artwork && downloadArtwork) {
        m_artwork->downloadArtwork(localGameId, savedArtworkUrl);
        finishCurrent(true,
            QStringLiteral("Saved metadata found; retrying its artwork download."));
        return;
    }
    if (hasMetadata && !savedArtworkUrl.isEmpty() && !downloadArtwork) {
        finishCurrent(true,
            QStringLiteral("Saved metadata found; automatic artwork downloads are disabled."));
        return;
    }

    const QString platform =
        m_currentGame.value(QStringLiteral("platform")).toString();
    // Library-wide passes stop asking TheGamesDB when its allowance is low
    // or used up. Those games are recorded as "provider_error" and retried
    // by a later automatic pass.
    if (!m_singleGameRun && !m_theGamesDbUnavailable && m_metadata->isConfigured()
            && !m_metadata->automaticLookupsAllowed()) {
        m_theGamesDbUnavailable = true;
        m_theGamesDbFailure = m_metadata->automaticPauseReason();
        qCInfo(lcMetadata) << "Automatic TheGamesDB lookups paused:" << m_theGamesDbFailure;
    }
    const bool theGamesDbReady = m_metadata->isConfigured() && !m_theGamesDbUnavailable;
    const bool launchBoxFirst = !m_settings
        || m_settings->metadataProviderOrder() == QStringLiteral("launchbox");

    // LaunchBox goes first when it is the preferred provider, or when it is
    // the only one available. If it has no match, TheGamesDB is asked next.
    if (launchBoxAvailable() && (launchBoxFirst || !theGamesDbReady)) {
        QString message;
        const LaunchBoxOutcome outcome = attemptLaunchBoxMatch(&message);
        if (outcome == LaunchBoxOutcome::Matched) {
            markCurrent(QStringLiteral("matched"));
            finishCurrent(true, message);
            return;
        }
        if (!theGamesDbReady) {
            markCurrent(m_theGamesDbUnavailable ? QStringLiteral("provider_error")
                : outcome == LaunchBoxOutcome::Ambiguous ? QStringLiteral("ambiguous")
                : QStringLiteral("no_match"));
            finishCurrent(false, message);
            return;
        }
    }
    if (!theGamesDbReady) {
        markCurrent(m_theGamesDbUnavailable ? QStringLiteral("provider_error")
                                            : QStringLiteral("no_match"));
        finishCurrent(false, m_theGamesDbUnavailable
            ? QStringLiteral("TheGamesDB unavailable and LaunchBox is not installed.")
            : QStringLiteral("No metadata provider is available."));
        return;
    }

    const int platformId = m_metadata->metadataPlatformId(platform);
    if (platformId <= 0) {
        if (tryLaunchBoxFallback(QStringLiteral("TheGamesDB has no platform mapping.")))
            return;
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

    const QString localTitle =
        m_currentGame.value(QStringLiteral("title")).toString();
    const QString searchTitle = normalizedTitle(localTitle);
    setStatus(QStringLiteral("Checking %1...")
                  .arg(localTitle));
    qCDebug(lcMetadata) << "Searching TheGamesDB using normalized title:"
                       << searchTitle << "(local title:" << localTitle << ")";
    // Normalize only the provider query. The local game title and ROM path
    // remain unchanged, including any region/revision tags in the filename.
    m_metadata->searchGames(searchTitle, platformId);
}

void LibraryMetadataMatcher::finishCurrent(bool matched, const QString &reason)
{
    if (matched)
        ++m_matchedCount;
    else
        ++m_unresolvedCount;

    ++m_completedCount;
    // Find game info on one game: say how it ended (a game already matched
    // with LaunchBox keeps its match, which otherwise leaves no trace).
    if (m_singleGameRun && !reason.isEmpty())
        qCInfo(lcMetadata).noquote() << "Find game info for"
            << m_currentGame.value(QStringLiteral("title")).toString() << "finished:" << reason;
    m_currentGame.clear();
    emit progressChanged();
    if (!reason.isEmpty())
        setStatus(QStringLiteral("%1 (%2/%3)")
                      .arg(reason).arg(m_completedCount).arg(m_totalCount));
    // The next game after a pass through the event loop, so the window keeps
    // drawing and answering input between games: LaunchBox matches finish
    // without waiting on the network, and matching the whole library in one
    // go froze the interface on first start.
    if (m_queue.isEmpty())
        processNext();
    else
        QTimer::singleShot(0, this, &LibraryMetadataMatcher::processNext);
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
