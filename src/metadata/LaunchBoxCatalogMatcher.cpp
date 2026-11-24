#include "LaunchBoxCatalogMatcher.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace {

QString normalizedPlatform(const QString &platform)
{
    QString result;
    for (const QChar character : platform) {
        if (character.isLetterOrNumber())
            result.append(character.toCaseFolded());
    }
    return result;
}

QString titleWithoutExtension(const QString &value)
{
    const QFileInfo info(value);
    const QString suffix = info.suffix();
    static const QStringList knownExtensions = {
        QStringLiteral("z64"), QStringLiteral("n64"), QStringLiteral("v64"),
        QStringLiteral("iso"), QStringLiteral("rvz"), QStringLiteral("gcm"),
        QStringLiteral("cue"), QStringLiteral("bin"), QStringLiteral("pbp"),
        QStringLiteral("chd"), QStringLiteral("xiso")
    };
    if (!suffix.isEmpty() && knownExtensions.contains(suffix.toCaseFolded()))
        return info.completeBaseName();
    return info.fileName();
}

LaunchBoxMatchResult resultForCandidates(const QList<int> &matches,
                                        const QString &description)
{
    if (matches.isEmpty()) {
        return {LaunchBoxMatchResult::Status::NoMatch, -1,
                QStringLiteral("No %1 match was found.").arg(description)};
    }
    if (matches.size() != 1) {
        return {LaunchBoxMatchResult::Status::Ambiguous, -1,
                QStringLiteral("Multiple %1 matches were found.").arg(description)};
    }
    return {LaunchBoxMatchResult::Status::Matched, matches.first(), {}};
}

} // namespace

QString LaunchBoxCatalogMatcher::canonicalPlatformForInternalName(
    const QString &internalName)
{
    const QString key = normalizedPlatform(internalName);
    if (key == QStringLiteral("gb"))
        return QStringLiteral("Nintendo Game Boy");
    if (key == QStringLiteral("gbc"))
        return QStringLiteral("Nintendo Game Boy Color");
    if (key == QStringLiteral("gba"))
        return QStringLiteral("Nintendo Game Boy Advance");
    if (key == QStringLiteral("snes"))
        return QStringLiteral("Super Nintendo Entertainment System");
    if (key == QStringLiteral("n64"))
        return QStringLiteral("Nintendo 64");
    if (key == QStringLiteral("ps1"))
        return QStringLiteral("Sony Playstation");
    if (key == QStringLiteral("ps2"))
        return QStringLiteral("Sony Playstation 2");
    if (key == QStringLiteral("wiiu"))
        return QStringLiteral("Nintendo Wii U");
    if (key == QStringLiteral("gamecube"))
        return QStringLiteral("Nintendo GameCube");
    if (key == QStringLiteral("wii"))
        return QStringLiteral("Nintendo Wii");
    if (key == QStringLiteral("xbox"))
        return QStringLiteral("Microsoft Xbox");
    if (key == QStringLiteral("xbox360"))
        return QStringLiteral("Microsoft Xbox 360");
    return {};
}

QString LaunchBoxCatalogMatcher::normalizedFileName(const QString &input)
{
    const QString fileName = titleWithoutExtension(input).trimmed()
                                .normalized(QString::NormalizationForm_D);
    QString result;
    result.reserve(fileName.size());
    for (const QChar character : fileName) {
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

QString LaunchBoxCatalogMatcher::normalizedTitleWithoutRevision(const QString &title)
{
    static const QRegularExpression revisionTag(
        QStringLiteral(R"(\s*[\(\[]\s*(?:Rev(?:ision)?|v)\s*[A-Z0-9.]+\s*[\)\]])"),
        QRegularExpression::CaseInsensitiveOption);
    QString value = title;
    value.remove(revisionTag);
    return normalizedTitle(value);
}

QString LaunchBoxCatalogMatcher::normalizedTitle(const QString &input)
{
    QString title = titleWithoutExtension(input).trimmed();

    static const QRegularExpression trailingArticle(
        QStringLiteral(R"(^\s*(.+?),\s*The\s*[-:–—]\s*(.+?)\s*$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch articleMatch = trailingArticle.match(title);
    if (articleMatch.hasMatch()) {
        title = QStringLiteral("The %1 %2")
                    .arg(articleMatch.captured(1), articleMatch.captured(2));
    }

    // Region and language markers describe the ROM dump, not the canonical
    // game title. Remove them while retaining revision tags for the first,
    // most-specific matching pass.
    static const QRegularExpression regionTag(
        // One or more regions: "(USA)", "(Japan, USA)", "(USA, Europe)".
        QStringLiteral(R"(\s*[\(\[]\s*(?:USA|US|U|Europe|EUR|EU|E|Japan|JPN|JP|J|World|W|Asia|Australia|Brazil|Canada|China|France|Germany|Hong Kong|Italy|Korea|Netherlands|Scandinavia|Spain|Sweden|Taiwan|UK|Latin America)(?:\s*[,;+]\s*(?:USA|US|U|Europe|EUR|EU|E|Japan|JPN|JP|J|World|W|Asia|Australia|Brazil|Canada|China|France|Germany|Hong Kong|Italy|Korea|Netherlands|Scandinavia|Spain|Sweden|Taiwan|UK|Latin America))*\s*[\)\]])"),
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

LaunchBoxMatchResult LaunchBoxCatalogMatcher::matchByFileAssociation(
    const QString &internalPlatform,
    const QString &localFileName,
    const QList<LaunchBoxFileAssociation> &associations,
    const QList<LaunchBoxGameRecord> &games)
{
    const QString canonical = canonicalPlatformForInternalName(internalPlatform);
    if (canonical.isEmpty()) {
        return {LaunchBoxMatchResult::Status::UnsupportedPlatform, -1,
                QStringLiteral("No explicit LaunchBox platform mapping exists for %1.")
                    .arg(internalPlatform)};
    }

    const QString wantedFile = normalizedFileName(localFileName);
    if (wantedFile.isEmpty())
        return {LaunchBoxMatchResult::Status::NoMatch, -1,
                QStringLiteral("The local ROM filename is empty.")};

    QList<QString> associatedGameNames;
    for (const LaunchBoxFileAssociation &association : associations) {
        if (normalizedPlatform(association.platform)
                != normalizedPlatform(canonical)
            || normalizedFileName(association.fileName) != wantedFile) {
            continue;
        }

        const QString gameKey = normalizedFileName(association.gameName);
        if (gameKey.isEmpty())
            continue;
        bool alreadyPresent = false;
        for (const QString &existing : associatedGameNames) {
            if (normalizedFileName(existing) == gameKey) {
                alreadyPresent = true;
                break;
            }
        }
        if (!alreadyPresent)
            associatedGameNames.append(association.gameName);
    }

    if (associatedGameNames.isEmpty()) {
        return {LaunchBoxMatchResult::Status::NoMatch, -1,
                QStringLiteral("No platform-specific filename association was found.")};
    }
    if (associatedGameNames.size() > 1) {
        return {LaunchBoxMatchResult::Status::Ambiguous, -1,
                QStringLiteral("The ROM filename maps to multiple LaunchBox game names.")};
    }

    const QString wantedGame = normalizedTitle(associatedGameNames.first());
    QList<int> metadataMatches;
    for (int i = 0; i < games.size(); ++i) {
        const LaunchBoxGameRecord &game = games.at(i);
        if (normalizedPlatform(game.platform) != normalizedPlatform(canonical))
            continue;
        if (normalizedTitle(game.name) == wantedGame)
            metadataMatches.append(i);
    }

    // Some LaunchBox Files.xml GameName values retain a ROM revision suffix
    // while Metadata.xml Name stores only the canonical title. Use a
    // revision-insensitive association only as a secondary fallback, and
    // accept it only when exactly one record exists on the mapped platform.
    if (metadataMatches.isEmpty()) {
        const QString wantedBase = normalizedTitleWithoutRevision(
            associatedGameNames.first());
        for (int i = 0; i < games.size(); ++i) {
            const LaunchBoxGameRecord &game = games.at(i);
            if (normalizedPlatform(game.platform) == normalizedPlatform(canonical)
                && normalizedTitleWithoutRevision(game.name) == wantedBase) {
                metadataMatches.append(i);
            }
        }
    }

    return resultForCandidates(metadataMatches,
                               QStringLiteral("platform-specific metadata"));
}

LaunchBoxMatchResult LaunchBoxCatalogMatcher::matchByTitle(
    const QString &internalPlatform,
    const QString &localTitle,
    const QList<LaunchBoxGameRecord> &games)
{
    const QString canonical = canonicalPlatformForInternalName(internalPlatform);
    if (canonical.isEmpty()) {
        return {LaunchBoxMatchResult::Status::UnsupportedPlatform, -1,
                QStringLiteral("No explicit LaunchBox platform mapping exists for %1.")
                    .arg(internalPlatform)};
    }

    const QString wanted = normalizedTitle(localTitle);
    if (wanted.isEmpty())
        return {LaunchBoxMatchResult::Status::NoMatch, -1,
                QStringLiteral("The local game title is empty.")};

    QList<int> matches;
    for (int i = 0; i < games.size(); ++i) {
        const LaunchBoxGameRecord &game = games.at(i);
        if (normalizedPlatform(game.platform) != normalizedPlatform(canonical))
            continue;
        if (normalizedTitle(game.name) == wanted)
            matches.append(i);
    }

    // Prefer exact revision-preserving title matches. Only when none exist may
    // revision/version suffixes be ignored, and then only one platform-specific
    // candidate is safe to associate.
    if (!matches.isEmpty())
        return resultForCandidates(matches, QStringLiteral("exact platform-and-title"));

    const QString wantedWithoutRevision =
        normalizedTitleWithoutRevision(localTitle);
    for (int i = 0; i < games.size(); ++i) {
        const LaunchBoxGameRecord &game = games.at(i);
        if (normalizedPlatform(game.platform) != normalizedPlatform(canonical))
            continue;
        if (normalizedTitleWithoutRevision(game.name) == wantedWithoutRevision)
            matches.append(i);
    }
    return resultForCandidates(matches, QStringLiteral("revision-insensitive platform-and-title"));
}
